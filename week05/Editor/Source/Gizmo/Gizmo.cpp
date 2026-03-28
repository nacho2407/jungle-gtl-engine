#include "Gizmo.h"

#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Component/SceneComponent.h"
#include "Math/Transform.h"
#include "Picking/Picker.h"
#include "Primitive/PrimitiveGizmo.h"
#include "Primitive/UnrealEditorStyledGizmo.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Scene/Scene.h"
#include "Math/MathUtility.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	constexpr float TranslationAxisLengthUnits = 47.0f;
	constexpr float ScaleAxisLengthUnits = 25.0f;
	constexpr float ScaleReferenceUnits = 20.0f;
	constexpr float UniformScalePixelsPerUnit = 120.0f;
	constexpr float MinScaleMagnitude = 0.01f;
	constexpr float MaxScaleMagnitude = 1000.0f;
	constexpr float MinGizmoScale = 0.05f;
	constexpr float MaxGizmoScale = 3.0f;
	constexpr float GizmoViewportHeightRatio = 0.30f;
	constexpr float ParallelTolerance = 1.0e-6f;
	const FVector4 ActiveAxisColor = { 1.0f, 1.0f, 0.0f, 1.0f };

	float ClampScaleComponent(float Value)
	{
		float ClampedValue = std::clamp(Value, -MaxScaleMagnitude, MaxScaleMagnitude);
		if (std::abs(ClampedValue) < MinScaleMagnitude)
		{
			ClampedValue = (ClampedValue < 0.0f) ? -MinScaleMagnitude : MinScaleMagnitude;
		}

		return ClampedValue;
	}

	FVector ClampScaleVector(const FVector& InScale)
	{
		return FVector(
			ClampScaleComponent(InScale.X),
			ClampScaleComponent(InScale.Y),
			ClampScaleComponent(InScale.Z));
	}
}

CGizmo::CGizmo()
{
	// TODO: 매직 넘버 제거
	const FString GizmoMaterialName = "M_Gizmos";
	Material = FMaterialManager::Get().FindByName(GizmoMaterialName);
}

void CGizmo::SetMode(EGizmoMode InMode)
{
	Mode = InMode;
	ClearHover();
	EndDrag();
}

void CGizmo::SetCoordinateSpace(EGizmoCoordinateSpace InSpace)
{
	if (CoordinateSpace == InSpace)
	{
		return;
	}

	CoordinateSpace = InSpace;
	ClearHover();
	EndDrag();
}

void CGizmo::ToggleCoordinateSpace()
{
	SetCoordinateSpace(
		CoordinateSpace == EGizmoCoordinateSpace::World
		? EGizmoCoordinateSpace::Local
		: EGizmoCoordinateSpace::World);
}

void CGizmo::CycleMode()
{
	switch (Mode)
	{
	case EGizmoMode::Location:
		Mode = EGizmoMode::Rotation;
		break;
	case EGizmoMode::Rotation:
		Mode = EGizmoMode::Scale;
		break;
	case EGizmoMode::Scale:
	default:
		Mode = EGizmoMode::Location;
		break;
	}

	ClearHover();
	EndDrag();
}

void CGizmo::BuildRenderCommands(AActor* SelectedActor, const CCamera* Camera, FRenderCommandQueue& OutQueue) const
{
	if (!SelectedActor || SelectedActor->IsPendingDestroy() || !Camera)
	{
		return;
	}

	const FVector WorldLocation = GetActorWorldLocation(SelectedActor);
	const float GizmoScale = ComputeGizmoScale(WorldLocation, Camera);
	const float RenderGizmoScale = GetRenderGizmoScale(GizmoScale);
	const FQuat GizmoRotation = GetGizmoRotation(SelectedActor);
	const FMatrix AxisGizmoWorld = FTransform(GizmoRotation, WorldLocation, FVector(RenderGizmoScale, RenderGizmoScale, RenderGizmoScale)).ToMatrixWithScale();
	const FMatrix ScreenGizmoWorld = FTransform(FQuat::Identity, WorldLocation, FVector(RenderGizmoScale, RenderGizmoScale, RenderGizmoScale)).ToMatrixWithScale();
	FRenderCommand Command;
	Command.WorldMatrix = AxisGizmoWorld;
	Command.RenderLayer = ERenderLayer::Overlay;
	Command.Material = Material.get();
	Command.bDisableDepthTest = true;
	Command.bDisableDepthWrite = true;
	Command.bDisableCulling = true;
	

	switch (Mode)
	{
	case EGizmoMode::Location:
		if (!EnsureTranslationMeshes())
		{
			return;
		}
		Command.MeshData = TranslationGizmo->GetMeshData();
		OutQueue.AddCommand(Command);
		break;

	case EGizmoMode::Rotation:
		if (!EnsureRotationMeshes(Camera, WorldLocation))
		{
			return;
		}
		for (const std::shared_ptr<FMeshData>& AxisMesh : RotationAxisMeshes)
		{
			if (!AxisMesh)
			{
				continue;
			}

			Command.WorldMatrix = AxisGizmoWorld;
			Command.MeshData = AxisMesh.get();
			OutQueue.AddCommand(Command);
		}

		if (RotationScreenMesh)
		{
			Command.WorldMatrix = ScreenGizmoWorld;
			Command.MeshData = RotationScreenMesh.get();
			OutQueue.AddCommand(Command);
		}
		break;

	case EGizmoMode::Scale:
		if (!EnsureScaleMeshes())
		{
			return;
		}
		Command.MeshData = ScaleGizmo->GetMeshData();
		OutQueue.AddCommand(Command);
		break;

	default:
		return;
	}

	const EGizmoAxis DisplayAxis = GetDisplayAxis();
	if (DisplayAxis != EGizmoAxis::None)
	{
		auto AddHighlightCommand = [&](std::shared_ptr<FMeshData>& HighlightMeshSlot, const FMatrix& HighlightWorldMatrix, auto&& Factory)
		{
			if (!HighlightMeshSlot)
			{
				HighlightMeshSlot = Factory();
			}

			if (HighlightMeshSlot)
			{
				FRenderCommand HighlightCommand = Command;
				HighlightCommand.WorldMatrix = HighlightWorldMatrix;
				HighlightCommand.MeshData = HighlightMeshSlot.get();
				OutQueue.AddCommand(HighlightCommand);
			}
		};

		auto AddTranslationAxisHighlight = [&](EAxis Axis)
		{
			const int32 AxisIndex = static_cast<int32>(Axis);
			AddHighlightCommand(
				HighlightTranslationAxes[AxisIndex],
				AxisGizmoWorld,
				[Axis]()
				{
					return CPrimitiveGizmo::CreateTranslationAxisMesh(Axis, ActiveAxisColor);
				});
		};

		auto AddRotationAxisHighlight = [&](EAxis Axis)
		{
			const int32 AxisIndex = static_cast<int32>(Axis);
			AddHighlightCommand(
				HighlightRotationAxes[AxisIndex],
				AxisGizmoWorld,
				[this, Camera, WorldLocation, Axis]()
				{
					return CPrimitiveGizmo::CreateRotationAxisMesh(Axis, BuildRotationDesc(Camera, WorldLocation), ActiveAxisColor);
				});
		};

		auto AddScaleAxisHighlight = [&](EAxis Axis)
		{
			const int32 AxisIndex = static_cast<int32>(Axis);
			AddHighlightCommand(
				HighlightScaleAxes[AxisIndex],
				AxisGizmoWorld,
				[Axis]()
				{
					return CPrimitiveGizmo::CreateScaleAxisMesh(Axis, ActiveAxisColor);
				});
		};

		if (DisplayAxis >= EGizmoAxis::X && DisplayAxis <= EGizmoAxis::Z)
		{
			const int32 AxisIndex = static_cast<int32>(DisplayAxis) - 1;
			if (Mode == EGizmoMode::Location)
			{
				AddTranslationAxisHighlight(static_cast<EAxis>(AxisIndex));
			}
			else if (Mode == EGizmoMode::Rotation)
			{
				AddRotationAxisHighlight(static_cast<EAxis>(AxisIndex));
			}
			else if (Mode == EGizmoMode::Scale)
			{
				AddScaleAxisHighlight(static_cast<EAxis>(AxisIndex));
			}
		}
		else if (Mode == EGizmoMode::Location && DisplayAxis >= EGizmoAxis::XY && DisplayAxis <= EGizmoAxis::YZ)
		{
			const int32 PlaneIndex = static_cast<int32>(DisplayAxis) - static_cast<int32>(EGizmoAxis::XY);
			AddHighlightCommand(
				HighlightTranslationPlanes[PlaneIndex],
				AxisGizmoWorld,
				[PlaneIndex]()
				{
					return CPrimitiveGizmo::CreateTranslationPlaneMesh(
						static_cast<CPrimitiveGizmo::ETranslationPlane>(PlaneIndex),
						ActiveAxisColor);
				});

			switch (DisplayAxis)
			{
			case EGizmoAxis::XY:
				AddTranslationAxisHighlight(EAxis::X);
				AddTranslationAxisHighlight(EAxis::Y);
				break;

			case EGizmoAxis::XZ:
				AddTranslationAxisHighlight(EAxis::X);
				AddTranslationAxisHighlight(EAxis::Z);
				break;

			case EGizmoAxis::YZ:
				AddTranslationAxisHighlight(EAxis::Y);
				AddTranslationAxisHighlight(EAxis::Z);
				break;

			default:
				break;
			}
		}
		else if (Mode == EGizmoMode::Scale && DisplayAxis >= EGizmoAxis::XY && DisplayAxis <= EGizmoAxis::YZ)
		{
			const int32 PlaneIndex = static_cast<int32>(DisplayAxis) - static_cast<int32>(EGizmoAxis::XY);
			AddHighlightCommand(
				HighlightScalePlanes[PlaneIndex],
				AxisGizmoWorld,
				[PlaneIndex]()
				{
					return CPrimitiveGizmo::CreateScalePlaneMesh(
						static_cast<CPrimitiveGizmo::EScalePlane>(PlaneIndex),
						ActiveAxisColor);
				});

			switch (DisplayAxis)
			{
			case EGizmoAxis::XY:
				AddScaleAxisHighlight(EAxis::X);
				AddScaleAxisHighlight(EAxis::Y);
				break;

			case EGizmoAxis::XZ:
				AddScaleAxisHighlight(EAxis::X);
				AddScaleAxisHighlight(EAxis::Z);
				break;

			case EGizmoAxis::YZ:
				AddScaleAxisHighlight(EAxis::Y);
				AddScaleAxisHighlight(EAxis::Z);
				break;

			default:
				break;
			}
		}
		else if (Mode == EGizmoMode::Scale && DisplayAxis == EGizmoAxis::XYZ)
		{
			AddHighlightCommand(
				HighlightScaleCenterMesh,
				AxisGizmoWorld,
				[]()
				{
					return CPrimitiveGizmo::CreateScaleCenterMesh(ActiveAxisColor);
				});
		}
		else if (Mode == EGizmoMode::Location && DisplayAxis == EGizmoAxis::Screen)
		{
			AddHighlightCommand(
				HighlightTranslationScreenMesh,
				AxisGizmoWorld,
				[]()
				{
					return CPrimitiveGizmo::CreateTranslationScreenMesh(ActiveAxisColor);
				});
		}
		else if (Mode == EGizmoMode::Rotation && DisplayAxis == EGizmoAxis::Screen)
		{
			AddHighlightCommand(
				HighlightRotationScreenMesh,
				ScreenGizmoWorld,
				[this, Camera, WorldLocation]()
				{
					return CPrimitiveGizmo::CreateRotationScreenMesh(BuildRotationDesc(Camera, WorldLocation), ActiveAxisColor);
				});
		}
	}
}

bool CGizmo::BeginDrag(AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight)
{
	if (!SelectedActor || SelectedActor->IsPendingDestroy())
	{
		return false;
	}

	const EGizmoAxis Axis = HitTestAxis(SelectedActor, Scene, Picker, ScreenX, ScreenY, ScreenWidth, ScreenHeight);
	if (Axis == EGizmoAxis::None)
	{
		return false;
	}

	return BeginAxisDrag(Axis, SelectedActor, Scene, Picker, ScreenX, ScreenY, ScreenWidth, ScreenHeight);
}

bool CGizmo::UpdateDrag(AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight)
{
	if (ActiveAxis == EGizmoAxis::None || !SelectedActor || SelectedActor->IsPendingDestroy() || !Scene || !Scene->GetCamera())
	{
		return false;
	}

	const FRay Ray = Picker.ScreenToRay(Scene->GetCamera(), ScreenX, ScreenY, ScreenWidth, ScreenHeight);

	FVector Intersection = FVector::ZeroVector;
	if (!IntersectPlane(Ray, DragStartGizmoLocation, DragPlaneNormal, Intersection))
	{
		return false;
	}

	if (Mode == EGizmoMode::Location)
	{
		FVector NewWorldLocation = DragStartActorLocation;
		if (ActiveAxis >= EGizmoAxis::X && ActiveAxis <= EGizmoAxis::Z)
		{
			const FVector Axis = GetGizmoAxisVector(ActiveAxis, SelectedActor);
			const float AxisDistance = FVector::DotProduct(Intersection - DragStartGizmoLocation, Axis);
			NewWorldLocation = DragStartActorLocation + Axis * (AxisDistance - DragStartAxisDistance);
		}
		else
		{
			NewWorldLocation = DragStartActorLocation + (Intersection - DragStartIntersection);
		}

		return ApplyActorWorldLocation(SelectedActor, NewWorldLocation);
	}

	if (Mode == EGizmoMode::Rotation)
	{
		const FVector CurrentVector = (Intersection - DragStartGizmoLocation).GetSafeNormal();
		if (CurrentVector.IsNearlyZero(ParallelTolerance) || DragStartRotationVector.IsNearlyZero(ParallelTolerance))
		{
			return false;
		}

		const FVector Axis = DragPlaneNormal.GetSafeNormal();
		if (Axis.IsNearlyZero(ParallelTolerance))
		{
			return false;
		}
		const FVector Cross = FVector::CrossProduct(DragStartRotationVector, CurrentVector);
		const float SignedAngleRadians = std::atan2(
			FVector::DotProduct(Cross, Axis),
			FVector::DotProduct(DragStartRotationVector, CurrentVector));
		CurrentRotationDeltaDegrees = FMath::RadiansToDegrees(SignedAngleRadians);
		const FQuat DeltaRotation(Axis, SignedAngleRadians);
		return ApplyActorWorldRotation(SelectedActor, (DragStartActorRotation * DeltaRotation).GetNormalized());
	}

	if (Mode == EGizmoMode::Scale)
	{
		FVector NewScale = DragStartActorScale;
		const float GizmoScale = GetRenderGizmoScale(ComputeGizmoScale(DragStartGizmoLocation, Scene->GetCamera()));
		const float ScaleDenominator = (ScaleReferenceUnits * GizmoScale > ParallelTolerance)
			? (ScaleReferenceUnits * GizmoScale)
			: ScaleReferenceUnits;

		if (ActiveAxis >= EGizmoAxis::X && ActiveAxis <= EGizmoAxis::Z)
		{
			const FVector Axis = GetGizmoAxisVector(ActiveAxis, SelectedActor);
			const float CurrentAxisDistance = FVector::DotProduct(Intersection - DragStartGizmoLocation, Axis);
			const float DeltaScale = (CurrentAxisDistance - DragStartAxisDistance) / ScaleDenominator;
			const int32 AxisIndex = static_cast<int32>(ActiveAxis) - 1;
			NewScale[AxisIndex] = ClampScaleComponent(DragStartActorScale[AxisIndex] + DeltaScale);
		}
		else if (ActiveAxis >= EGizmoAxis::XY && ActiveAxis <= EGizmoAxis::YZ)
		{
			const FVector Offset = Intersection - DragStartIntersection;
			switch (ActiveAxis)
			{
			case EGizmoAxis::XY:
			{
				const float DeltaX = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::X, SelectedActor)) / ScaleDenominator;
				const float DeltaY = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::Y, SelectedActor)) / ScaleDenominator;
				NewScale.X = ClampScaleComponent(DragStartActorScale.X + DeltaX);
				NewScale.Y = ClampScaleComponent(DragStartActorScale.Y + DeltaY);
				break;
			}

			case EGizmoAxis::XZ:
			{
				const float DeltaX = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::X, SelectedActor)) / ScaleDenominator;
				const float DeltaZ = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::Z, SelectedActor)) / ScaleDenominator;
				NewScale.X = ClampScaleComponent(DragStartActorScale.X + DeltaX);
				NewScale.Z = ClampScaleComponent(DragStartActorScale.Z + DeltaZ);
				break;
			}

			case EGizmoAxis::YZ:
			{
				const float DeltaY = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::Y, SelectedActor)) / ScaleDenominator;
				const float DeltaZ = FVector::DotProduct(Offset, GetGizmoAxisVector(EGizmoAxis::Z, SelectedActor)) / ScaleDenominator;
				NewScale.Y = ClampScaleComponent(DragStartActorScale.Y + DeltaY);
				NewScale.Z = ClampScaleComponent(DragStartActorScale.Z + DeltaZ);
				break;
			}

			default:
				break;
			}
		}
		else if (ActiveAxis == EGizmoAxis::XYZ)
		{
			const float PixelDelta = static_cast<float>((ScreenX - DragStartScreenX) - (ScreenY - DragStartScreenY));
			const float UniformDelta = PixelDelta / UniformScalePixelsPerUnit;
			NewScale = ClampScaleVector(DragStartActorScale + FVector(UniformDelta, UniformDelta, UniformDelta));
		}

		return ApplyActorRelativeScale(SelectedActor, ClampScaleVector(NewScale));
	}

	return false;
}

void CGizmo::UpdateHover(AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight)
{
	if (IsDragging())
	{
		return;
	}

	if ((Mode != EGizmoMode::Location && Mode != EGizmoMode::Rotation && Mode != EGizmoMode::Scale) || !SelectedActor || SelectedActor->IsPendingDestroy())
	{
		HoveredAxis = EGizmoAxis::None;
		return;
	}

	HoveredAxis = HitTestAxis(SelectedActor, Scene, Picker, ScreenX, ScreenY, ScreenWidth, ScreenHeight);
}

void CGizmo::ClearHover()
{
	HoveredAxis = EGizmoAxis::None;
}

void CGizmo::EndDrag()
{
	ActiveAxis = EGizmoAxis::None;
	CurrentRotationDeltaDegrees = 0.0f;
	DragStartActorLocation = FVector::ZeroVector;
	DragStartGizmoLocation = FVector::ZeroVector;
	DragStartIntersection = FVector::ZeroVector;
	DragPlaneNormal = FVector::ZeroVector;
	DragStartRotationVector = FVector::ZeroVector;
	DragStartActorScale = FVector::OneVector;
	DragStartAxisDistance = 0.0f;
	DragStartActorRotation = FQuat::Identity;
	DragStartScreenX = 0;
	DragStartScreenY = 0;
}

bool CGizmo::EnsureTranslationMeshes() const
{
	if (!TranslationGizmo || !TranslationGizmo->GetMeshData())
	{
		TranslationGizmo = std::make_unique<CPrimitiveGizmo>(CPrimitiveGizmo::EGizmoType::Translation);
	}

	if (!TranslationAxisMeshes[0])
	{
		TranslationAxisMeshes[0] = CPrimitiveGizmo::CreateTranslationAxisMesh(EAxis::X);
	}

	if (!TranslationAxisMeshes[1])
	{
		TranslationAxisMeshes[1] = CPrimitiveGizmo::CreateTranslationAxisMesh(EAxis::Y);
	}

	if (!TranslationAxisMeshes[2])
	{
		TranslationAxisMeshes[2] = CPrimitiveGizmo::CreateTranslationAxisMesh(EAxis::Z);
	}

	if (!TranslationPlaneMeshes[0])
	{
		TranslationPlaneMeshes[0] = CPrimitiveGizmo::CreateTranslationPlaneMesh(CPrimitiveGizmo::ETranslationPlane::XY);
	}

	if (!TranslationPlaneMeshes[1])
	{
		TranslationPlaneMeshes[1] = CPrimitiveGizmo::CreateTranslationPlaneMesh(CPrimitiveGizmo::ETranslationPlane::XZ);
	}

	if (!TranslationPlaneMeshes[2])
	{
		TranslationPlaneMeshes[2] = CPrimitiveGizmo::CreateTranslationPlaneMesh(CPrimitiveGizmo::ETranslationPlane::YZ);
	}

	if (!TranslationScreenMesh)
	{
		TranslationScreenMesh = CPrimitiveGizmo::CreateTranslationScreenMesh();
	}

	return TranslationGizmo
		&& TranslationGizmo->GetMeshData()
		&& TranslationAxisMeshes[0]
		&& TranslationAxisMeshes[1]
		&& TranslationAxisMeshes[2]
		&& TranslationPlaneMeshes[0]
		&& TranslationPlaneMeshes[1]
		&& TranslationPlaneMeshes[2]
		&& TranslationScreenMesh;
}

bool CGizmo::EnsureRotationMeshes(const CCamera* Camera, const FVector& GizmoWorldLocation) const
{
	if (!Camera)
	{
		return false;
	}

	const RotationDesc Desc = BuildRotationDesc(Camera, GizmoWorldLocation);
	const bool bViewChanged =
		!CachedRotationCameraDirection.Equals(Desc.cameraDirection, 1.0e-4f) ||
		!CachedRotationViewUp.Equals(Desc.viewUp, 1.0e-4f) ||
		!CachedRotationViewRight.Equals(Desc.viewRight, 1.0e-4f);
	const bool bShapeStateChanged =
		(CachedRotationDragging != Desc.dragging) ||
		(CachedRotationActiveAxis != ActiveAxis) ||
		Desc.dragging;

	if (bViewChanged || bShapeStateChanged || !RotationAxisMeshes[0])
	{
		RotationAxisMeshes[0] = CPrimitiveGizmo::CreateRotationAxisMesh(EAxis::X, Desc);
		RotationAxisMeshes[1] = CPrimitiveGizmo::CreateRotationAxisMesh(EAxis::Y, Desc);
		RotationAxisMeshes[2] = CPrimitiveGizmo::CreateRotationAxisMesh(EAxis::Z, Desc);
		RotationScreenMesh = CPrimitiveGizmo::CreateRotationScreenMesh(Desc);
		HighlightRotationAxes[0].reset();
		HighlightRotationAxes[1].reset();
		HighlightRotationAxes[2].reset();
		HighlightRotationScreenMesh.reset();
		CachedRotationCameraDirection = Desc.cameraDirection;
		CachedRotationViewUp = Desc.viewUp;
		CachedRotationViewRight = Desc.viewRight;
		CachedRotationDragging = Desc.dragging;
		CachedRotationActiveAxis = ActiveAxis;
	}

	return RotationAxisMeshes[0]
		|| RotationAxisMeshes[1]
		|| RotationAxisMeshes[2]
		|| RotationScreenMesh;
}

bool CGizmo::EnsureScaleMeshes() const
{
	if (!ScaleGizmo || !ScaleGizmo->GetMeshData())
	{
		ScaleGizmo = std::make_unique<CPrimitiveGizmo>(CPrimitiveGizmo::EGizmoType::Scale);
	}

	if (!ScaleAxisMeshes[0])
	{
		ScaleAxisMeshes[0] = CPrimitiveGizmo::CreateScaleAxisMesh(EAxis::X);
	}

	if (!ScaleAxisMeshes[1])
	{
		ScaleAxisMeshes[1] = CPrimitiveGizmo::CreateScaleAxisMesh(EAxis::Y);
	}

	if (!ScaleAxisMeshes[2])
	{
		ScaleAxisMeshes[2] = CPrimitiveGizmo::CreateScaleAxisMesh(EAxis::Z);
	}

	if (!ScalePlaneMeshes[0])
	{
		ScalePlaneMeshes[0] = CPrimitiveGizmo::CreateScalePlaneMesh(CPrimitiveGizmo::EScalePlane::XY);
	}

	if (!ScalePlaneMeshes[1])
	{
		ScalePlaneMeshes[1] = CPrimitiveGizmo::CreateScalePlaneMesh(CPrimitiveGizmo::EScalePlane::XZ);
	}

	if (!ScalePlaneMeshes[2])
	{
		ScalePlaneMeshes[2] = CPrimitiveGizmo::CreateScalePlaneMesh(CPrimitiveGizmo::EScalePlane::YZ);
	}

	if (!ScaleCenterMesh)
	{
		ScaleCenterMesh = CPrimitiveGizmo::CreateScaleCenterMesh();
	}

	return ScaleGizmo
		&& ScaleGizmo->GetMeshData()
		&& ScaleAxisMeshes[0]
		&& ScaleAxisMeshes[1]
		&& ScaleAxisMeshes[2]
		&& ScalePlaneMeshes[0]
		&& ScalePlaneMeshes[1]
		&& ScalePlaneMeshes[2]
		&& ScaleCenterMesh;
}

EGizmoAxis CGizmo::HitTestAxis(AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight) const
{
	if (!SelectedActor || SelectedActor->IsPendingDestroy() || !Scene || !Scene->GetCamera())
	{
		return EGizmoAxis::None;
	}

	const FVector WorldLocation = GetActorWorldLocation(SelectedActor);
	const std::shared_ptr<FMeshData>* AxisMeshes = nullptr;
	if (Mode == EGizmoMode::Location)
	{
		if (!EnsureTranslationMeshes())
		{
			return EGizmoAxis::None;
		}
		AxisMeshes = TranslationAxisMeshes;
	}
	else if (Mode == EGizmoMode::Rotation)
	{
		if (!EnsureRotationMeshes(Scene->GetCamera(), WorldLocation))
		{
			return EGizmoAxis::None;
		}
		AxisMeshes = RotationAxisMeshes;
	}
	else if (Mode == EGizmoMode::Scale)
	{
		if (!EnsureScaleMeshes())
		{
			return EGizmoAxis::None;
		}
		AxisMeshes = ScaleAxisMeshes;
	}

	if (!AxisMeshes)
	{
		return EGizmoAxis::None;
	}

	const FRay Ray = Picker.ScreenToRay(Scene->GetCamera(), ScreenX, ScreenY, ScreenWidth, ScreenHeight);
	const float GizmoScale = GetRenderGizmoScale(ComputeGizmoScale(WorldLocation, Scene->GetCamera()));
	const FQuat GizmoRotation = GetGizmoRotation(SelectedActor);
	const FMatrix AxisGizmoWorld = FTransform(GizmoRotation, WorldLocation, FVector(GizmoScale, GizmoScale, GizmoScale)).ToMatrixWithScale();
	const FMatrix ScreenGizmoWorld = FTransform(FQuat::Identity, WorldLocation, FVector(GizmoScale, GizmoScale, GizmoScale)).ToMatrixWithScale();

	EGizmoAxis BestAxis = EGizmoAxis::None;
	float BestDistance = (std::numeric_limits<float>::max)();

	const auto TestMesh = [&](const std::shared_ptr<FMeshData>& AxisMesh, const FMatrix& MeshWorld, EGizmoAxis Handle)
	{
		if (!AxisMesh)
		{
			return;
		}

		for (size_t TriangleIndex = 0; TriangleIndex + 2 < AxisMesh->Indices.size(); TriangleIndex += 3)
		{
			const uint32 Index0 = AxisMesh->Indices[TriangleIndex];
			const uint32 Index1 = AxisMesh->Indices[TriangleIndex + 1];
			const uint32 Index2 = AxisMesh->Indices[TriangleIndex + 2];
			if (Index0 >= AxisMesh->Vertices.size() || Index1 >= AxisMesh->Vertices.size() || Index2 >= AxisMesh->Vertices.size())
			{
				continue;
			}

			const FVector Vertex0 = MeshWorld.TransformPosition(AxisMesh->Vertices[Index0].Position);
			const FVector Vertex1 = MeshWorld.TransformPosition(AxisMesh->Vertices[Index1].Position);
			const FVector Vertex2 = MeshWorld.TransformPosition(AxisMesh->Vertices[Index2].Position);

			float HitDistance = 0.0f;
			if (RayTriangleIntersectTwoSided(Ray, Vertex0, Vertex1, Vertex2, HitDistance) && HitDistance < BestDistance)
			{
				BestDistance = HitDistance;
				BestAxis = Handle;
			}
		}
	};

	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		TestMesh(AxisMeshes[AxisIndex], AxisGizmoWorld, static_cast<EGizmoAxis>(AxisIndex + 1));
	}

	if (Mode == EGizmoMode::Location)
	{
		for (int32 PlaneIndex = 0; PlaneIndex < 3; ++PlaneIndex)
		{
			TestMesh(TranslationPlaneMeshes[PlaneIndex], AxisGizmoWorld, static_cast<EGizmoAxis>(static_cast<int32>(EGizmoAxis::XY) + PlaneIndex));
		}

		TestMesh(TranslationScreenMesh, AxisGizmoWorld, EGizmoAxis::Screen);
	}
	else if (Mode == EGizmoMode::Rotation)
	{
		TestMesh(RotationScreenMesh, ScreenGizmoWorld, EGizmoAxis::Screen);
	}
	else if (Mode == EGizmoMode::Scale)
	{
		for (int32 PlaneIndex = 0; PlaneIndex < 3; ++PlaneIndex)
		{
			TestMesh(ScalePlaneMeshes[PlaneIndex], AxisGizmoWorld, static_cast<EGizmoAxis>(static_cast<int32>(EGizmoAxis::XY) + PlaneIndex));
		}

		TestMesh(ScaleCenterMesh, AxisGizmoWorld, EGizmoAxis::XYZ);
	}

	return BestAxis;
}

bool CGizmo::BeginAxisDrag(EGizmoAxis AxisId, AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight)
{
	if (Mode == EGizmoMode::Location)
	{
		return BeginTranslationDrag(AxisId, SelectedActor, Scene, Picker, ScreenX, ScreenY, ScreenWidth, ScreenHeight);
	}

	if (Mode == EGizmoMode::Rotation)
	{
		return BeginRotationDrag(AxisId, SelectedActor, Scene, Picker, ScreenX, ScreenY, ScreenWidth, ScreenHeight);
	}

	if (Mode == EGizmoMode::Scale)
	{
		return BeginScaleDrag(AxisId, SelectedActor, Scene, Picker, ScreenX, ScreenY, ScreenWidth, ScreenHeight);
	}

	return false;
}

bool CGizmo::BeginTranslationDrag(EGizmoAxis AxisId, AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight)
{
	if (!SelectedActor || SelectedActor->IsPendingDestroy() || !Scene || !Scene->GetCamera())
	{
		return false;
	}

	const FRay Ray = Picker.ScreenToRay(Scene->GetCamera(), ScreenX, ScreenY, ScreenWidth, ScreenHeight);

	const FVector GizmoLocation = GetActorWorldLocation(SelectedActor);
	const FVector Axis = GetGizmoAxisVector(AxisId, SelectedActor);
	const CCamera* Camera = Scene->GetCamera();

	FVector PlaneNormal = FVector::ZeroVector;
	if (AxisId >= EGizmoAxis::X && AxisId <= EGizmoAxis::Z)
	{
		FVector PlaneTangent = FVector::CrossProduct(Camera->GetForward(), Axis);
		if (PlaneTangent.SizeSquared() <= ParallelTolerance)
		{
			PlaneTangent = FVector::CrossProduct(Camera->GetRight(), Axis);
		}
		if (PlaneTangent.SizeSquared() <= ParallelTolerance)
		{
			PlaneTangent = FVector::CrossProduct(FVector::UpVector, Axis);
		}

		PlaneNormal = FVector::CrossProduct(Axis, PlaneTangent).GetSafeNormal();
	}
	else if (AxisId == EGizmoAxis::Screen)
	{
		PlaneNormal = Camera->GetForward();
	}
	else
	{
		PlaneNormal = GetGizmoPlaneNormal(AxisId, SelectedActor);
	}

	if (PlaneNormal.SizeSquared() <= ParallelTolerance)
	{
		return false;
	}

	FVector Intersection = FVector::ZeroVector;
	if (!IntersectPlane(Ray, GizmoLocation, PlaneNormal, Intersection))
	{
		return false;
	}

	ActiveAxis = AxisId;
	DragStartActorLocation = GizmoLocation;
	DragStartGizmoLocation = GizmoLocation;
	DragStartIntersection = Intersection;
	DragPlaneNormal = PlaneNormal;
	DragStartAxisDistance = (AxisId >= EGizmoAxis::X && AxisId <= EGizmoAxis::Z)
		? FVector::DotProduct(Intersection - GizmoLocation, Axis)
		: 0.0f;
	return true;
}

bool CGizmo::BeginRotationDrag(EGizmoAxis AxisId, AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight)
{
	if (!SelectedActor || SelectedActor->IsPendingDestroy() || !Scene || !Scene->GetCamera())
	{
		return false;
	}

	const FRay Ray = Picker.ScreenToRay(Scene->GetCamera(), ScreenX, ScreenY, ScreenWidth, ScreenHeight);

	const FVector GizmoLocation = GetActorWorldLocation(SelectedActor);
	const FVector Axis = (AxisId == EGizmoAxis::Screen) ? Scene->GetCamera()->GetForward() : GetGizmoAxisVector(AxisId, SelectedActor);
	FVector Intersection = FVector::ZeroVector;
	if (!IntersectPlane(Ray, GizmoLocation, Axis, Intersection))
	{
		return false;
	}

	const FVector StartVector = (Intersection - GizmoLocation).GetSafeNormal();
	if (StartVector.IsNearlyZero(ParallelTolerance))
	{
		return false;
	}

	ActiveAxis = AxisId;
	DragStartActorLocation = GizmoLocation;
	DragStartGizmoLocation = GizmoLocation;
	DragPlaneNormal = Axis;
	DragStartRotationVector = StartVector;
	DragStartAxisDistance = 0.0f;
	DragStartActorRotation = GetActorWorldRotation(SelectedActor);
	return true;
}

bool CGizmo::BeginScaleDrag(EGizmoAxis AxisId, AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight)
{
	if (!SelectedActor || SelectedActor->IsPendingDestroy() || !Scene || !Scene->GetCamera())
	{
		return false;
	}

	const FRay Ray = Picker.ScreenToRay(Scene->GetCamera(), ScreenX, ScreenY, ScreenWidth, ScreenHeight);

	const FVector GizmoLocation = GetActorWorldLocation(SelectedActor);
	const CCamera* Camera = Scene->GetCamera();

	FVector PlaneNormal = FVector::ZeroVector;
	if (AxisId >= EGizmoAxis::X && AxisId <= EGizmoAxis::Z)
	{
		const FVector Axis = GetGizmoAxisVector(AxisId, SelectedActor);
		FVector PlaneTangent = FVector::CrossProduct(Camera->GetForward(), Axis);
		if (PlaneTangent.SizeSquared() <= ParallelTolerance)
		{
			PlaneTangent = FVector::CrossProduct(Camera->GetRight(), Axis);
		}
		if (PlaneTangent.SizeSquared() <= ParallelTolerance)
		{
			PlaneTangent = FVector::CrossProduct(FVector::UpVector, Axis);
		}

		PlaneNormal = FVector::CrossProduct(Axis, PlaneTangent).GetSafeNormal();
	}
	else if (AxisId >= EGizmoAxis::XY && AxisId <= EGizmoAxis::YZ)
	{
		PlaneNormal = GetGizmoPlaneNormal(AxisId, SelectedActor);
	}
	else if (AxisId == EGizmoAxis::XYZ)
	{
		PlaneNormal = Camera->GetForward();
	}

	if (PlaneNormal.SizeSquared() <= ParallelTolerance)
	{
		return false;
	}

	FVector Intersection = FVector::ZeroVector;
	if (!IntersectPlane(Ray, GizmoLocation, PlaneNormal, Intersection))
	{
		return false;
	}

	ActiveAxis = AxisId;
	DragStartActorLocation = GizmoLocation;
	DragStartGizmoLocation = GizmoLocation;
	DragStartIntersection = Intersection;
	DragPlaneNormal = PlaneNormal;
	DragStartActorScale = GetActorRelativeScale(SelectedActor);
	DragStartAxisDistance = (AxisId >= EGizmoAxis::X && AxisId <= EGizmoAxis::Z)
		? FVector::DotProduct(Intersection - GizmoLocation, GetGizmoAxisVector(AxisId, SelectedActor))
		: 0.0f;
	DragStartScreenX = ScreenX;
	DragStartScreenY = ScreenY;
	return true;
}

EGizmoAxis CGizmo::GetDisplayAxis() const
{
	return ActiveAxis != EGizmoAxis::None ? ActiveAxis : HoveredAxis;
}

FQuat CGizmo::GetGizmoRotation(const AActor* Actor) const
{
	const bool bUseLocalOrientation = (Mode == EGizmoMode::Scale) || (CoordinateSpace == EGizmoCoordinateSpace::Local);
	if (!bUseLocalOrientation)
	{
		return FQuat::Identity;
	}

	if (Mode == EGizmoMode::Scale)
	{
		return GetComponentWorldRotationIgnoringScale(Actor ? Actor->GetRootComponent() : nullptr);
	}

	return GetActorWorldRotation(Actor);
}

FVector CGizmo::GetGizmoAxisVector(EGizmoAxis Axis, const AActor* Actor) const
{
	const FVector WorldAxis = GetAxisVector(Axis);
	const bool bUseLocalOrientation = (Mode == EGizmoMode::Scale) || (CoordinateSpace == EGizmoCoordinateSpace::Local);
	if (!bUseLocalOrientation || WorldAxis.IsNearlyZero(ParallelTolerance))
	{
		return WorldAxis;
	}

	return GetGizmoRotation(Actor).RotateVector(WorldAxis).GetSafeNormal();
}

FVector CGizmo::GetGizmoPlaneNormal(EGizmoAxis Axis, const AActor* Actor) const
{
	const FVector WorldNormal = GetPlaneNormal(Axis);
	const bool bUseLocalOrientation = (Mode == EGizmoMode::Scale) || (CoordinateSpace == EGizmoCoordinateSpace::Local);
	if (!bUseLocalOrientation || WorldNormal.IsNearlyZero(ParallelTolerance))
	{
		return WorldNormal;
	}

	return GetGizmoRotation(Actor).RotateVector(WorldNormal).GetSafeNormal();
}

FVector CGizmo::GetAxisVector(EGizmoAxis Axis)
{
	switch (Axis)
	{
	case EGizmoAxis::X:
		return FVector::ForwardVector;
	case EGizmoAxis::Y:
		return FVector::RightVector;
	case EGizmoAxis::Z:
		return FVector::UpVector;
	default:
		return FVector::ZeroVector;
	}
}

RotationDesc CGizmo::BuildRotationDesc(const CCamera* Camera, const FVector& GizmoWorldLocation) const
{
	RotationDesc Desc{};
	if (!Camera)
	{
		return Desc;
	}

	const FVector Forward = Camera->GetForward().GetSafeNormal();
	const FVector Right = Camera->GetRight().GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
	const FVector CameraToGizmo = (GizmoWorldLocation - Camera->GetPosition()).GetSafeNormal();

	Desc.cameraDirection = CameraToGizmo.IsNearlyZero(ParallelTolerance) ? Forward : CameraToGizmo;
	Desc.viewUp = Up;
	Desc.viewRight = Right;
	Desc.fullAxisRings = false;
	Desc.includeInnerDisk = false;
	Desc.includeScreenRing = true;
	Desc.includeArcball = false;
	Desc.dragging = (Mode == EGizmoMode::Rotation && ActiveAxis != EGizmoAxis::None);
	Desc.deltaRotationDegrees = CurrentRotationDeltaDegrees;
	switch (ActiveAxis)
	{
	case EGizmoAxis::X:
		Desc.activeAxis = AxisId::X;
		break;
	case EGizmoAxis::Y:
		Desc.activeAxis = AxisId::Y;
		break;
	case EGizmoAxis::Z:
		Desc.activeAxis = AxisId::Z;
		break;
	case EGizmoAxis::Screen:
		Desc.activeAxis = AxisId::Screen;
		break;
	default:
		Desc.activeAxis = AxisId::None;
		break;
	}
	return Desc;
}

FVector CGizmo::GetPlaneNormal(EGizmoAxis Axis)
{
	switch (Axis)
	{
	case EGizmoAxis::XY:
		return FVector::UpVector;
	case EGizmoAxis::XZ:
		return FVector::RightVector;
	case EGizmoAxis::YZ:
		return FVector::ForwardVector;
	default:
		return FVector::ZeroVector;
	}
}

FVector CGizmo::GetActorWorldLocation(const AActor* Actor)
{
	if (!Actor || !Actor->GetRootComponent())
	{
		return FVector::ZeroVector;
	}

	return Actor->GetRootComponent()->GetWorldLocation();
}

FQuat CGizmo::GetActorWorldRotation(const AActor* Actor)
{
	if (!Actor || !Actor->GetRootComponent())
	{
		return FQuat::Identity;
	}

	return FTransform(Actor->GetRootComponent()->GetWorldTransform()).GetRotation();
}

FQuat CGizmo::GetComponentWorldRotationIgnoringScale(const USceneComponent* Component)
{
	if (!Component)
	{
		return FQuat::Identity;
	}

	const FQuat LocalRotation = Component->GetRelativeTransform().GetRotation();
	const USceneComponent* Parent = Component->GetAttachParent();
	if (!Parent)
	{
		return LocalRotation.GetNormalized();
	}

	return (LocalRotation * GetComponentWorldRotationIgnoringScale(Parent)).GetNormalized();
}

FVector CGizmo::GetActorRelativeScale(const AActor* Actor)
{
	if (!Actor || !Actor->GetRootComponent())
	{
		return FVector::OneVector;
	}

	return Actor->GetRootComponent()->GetRelativeTransform().GetScale3D();
}

bool CGizmo::ApplyActorWorldLocation(AActor* Actor, const FVector& NewWorldLocation)
{
	if (!Actor)
	{
		return false;
	}

	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (!RootComponent)
	{
		return false;
	}

	if (USceneComponent* AttachParent = RootComponent->GetAttachParent())
	{
		const FMatrix ParentInverse = AttachParent->GetWorldTransform().GetInverse();
		RootComponent->SetRelativeLocation(ParentInverse.TransformPosition(NewWorldLocation));
		return true;
	}

	RootComponent->SetRelativeLocation(NewWorldLocation);
	return true;
}

bool CGizmo::ApplyActorWorldRotation(AActor* Actor, const FQuat& NewWorldRotation)
{
	if (!Actor)
	{
		return false;
	}

	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (!RootComponent)
	{
		return false;
	}

	FTransform RelativeTransform = RootComponent->GetRelativeTransform();
	FQuat NewRelativeRotation = NewWorldRotation.GetNormalized();
	if (USceneComponent* AttachParent = RootComponent->GetAttachParent())
	{
		const FQuat ParentWorldRotation = FTransform(AttachParent->GetWorldTransform()).GetRotation();
		NewRelativeRotation = (NewWorldRotation * ParentWorldRotation.Inverse()).GetNormalized();
	}

	RelativeTransform.SetRotation(NewRelativeRotation);
	RootComponent->SetRelativeTransform(RelativeTransform);
	return true;
}

bool CGizmo::ApplyActorRelativeScale(AActor* Actor, const FVector& NewRelativeScale)
{
	if (!Actor)
	{
		return false;
	}

	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (!RootComponent)
	{
		return false;
	}

	FTransform RelativeTransform = RootComponent->GetRelativeTransform();
	RelativeTransform.SetScale3D(NewRelativeScale);
	RootComponent->SetRelativeTransform(RelativeTransform);
	return true;
}

bool CGizmo::IntersectPlane(const FRay& Ray, const FVector& PlaneOrigin, const FVector& PlaneNormal, FVector& OutIntersection)
{
	const float Denominator = FVector::DotProduct(PlaneNormal, Ray.Direction);
	if (std::abs(Denominator) <= ParallelTolerance)
	{
		return false;
	}

	const float RayParameter = FVector::DotProduct(PlaneOrigin - Ray.Origin, PlaneNormal) / Denominator;
	if (RayParameter < 0.0f)
	{
		return false;
	}

	OutIntersection = Ray.Origin + Ray.Direction * RayParameter;
	return true;
}

bool CGizmo::RayTriangleIntersectTwoSided(const FRay& Ray, const FVector& V0, const FVector& V1, const FVector& V2, float& OutDistance)
{
	const FVector Edge1 = V1 - V0;
	const FVector Edge2 = V2 - V0;
	const FVector H = FVector::CrossProduct(Ray.Direction, Edge2);
	const float A = FVector::DotProduct(Edge1, H);
	if (std::abs(A) <= ParallelTolerance)
	{
		return false;
	}

	const float F = 1.0f / A;
	const FVector S = Ray.Origin - V0;
	const float U = F * FVector::DotProduct(S, H);
	if (U < 0.0f || U > 1.0f)
	{
		return false;
	}

	const FVector Q = FVector::CrossProduct(S, Edge1);
	const float V = F * FVector::DotProduct(Ray.Direction, Q);
	if (V < 0.0f || U + V > 1.0f)
	{
		return false;
	}

	const float T = F * FVector::DotProduct(Edge2, Q);
	if (T <= ParallelTolerance)
	{
		return false;
	}

	OutDistance = T;
	return true;
}

float CGizmo::ComputeGizmoScale(const FVector& WorldPosition, const CCamera* Camera) const
{
	if (!Camera)
	{
		return MinGizmoScale;
	}

	float VisibleHeight = 0.0f;
	if (Camera->IsOrthographic())
	{
		VisibleHeight = Camera->GetOrthoHeight();
	}
	else
	{
		const float Distance = (WorldPosition - Camera->GetPosition()).Size();
		const float HalfFovRadians = FMath::DegreesToRadians(Camera->GetFOV() * 0.5f);
		VisibleHeight = 2.0f * (std::max)(Distance, 1.0f) * std::tan(HalfFovRadians);
	}

	const float DesiredAxisLength = VisibleHeight * GizmoViewportHeightRatio;
	const float ReferenceAxisLength = (Mode == EGizmoMode::Scale) ? ScaleAxisLengthUnits : TranslationAxisLengthUnits;
	return std::clamp(DesiredAxisLength / ReferenceAxisLength, MinGizmoScale, MaxGizmoScale);
}

float CGizmo::GetRenderGizmoScale(float BaseGizmoScale) const
{
	return (Mode == EGizmoMode::Scale) ? (BaseGizmoScale * 0.5f) : BaseGizmoScale;
}
