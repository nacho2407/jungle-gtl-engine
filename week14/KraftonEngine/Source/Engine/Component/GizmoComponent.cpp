#include "GizmoComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Collision/RayUtils.h"
#include "Render/Proxy/GizmoSceneProxy.h"
#include "Render/Scene/FScene.h"
#include <cfloat>

IMPLEMENT_CLASS(UGizmoComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UGizmoComponent)

FPrimitiveSceneProxy* UGizmoComponent::CreateSceneProxy()
{
	return new FGizmoSceneProxy(this, false); // Outer
}

void UGizmoComponent::CreateRenderState()
{
	if (SceneProxy) return;

	FScene* Scene = RegisteredScene;
	if (!Scene && Owner && Owner->GetWorld())
		Scene = &Owner->GetWorld()->GetScene();
	if (!Scene) return;

	// Outer 프록시 (기본 경로)
	SceneProxy = Scene->AddPrimitive(this);

	// Inner 프록시 (별도 등록)
	InnerProxy = new FGizmoSceneProxy(this, true);
	Scene->RegisterProxy(InnerProxy);
}

void UGizmoComponent::DestroyRenderState()
{
	FScene* Scene = RegisteredScene;
	if (!Scene && Owner && Owner->GetWorld())
		Scene = &Owner->GetWorld()->GetScene();

	if (Scene)
	{
		if (InnerProxy) { Scene->RemovePrimitive(InnerProxy); InnerProxy = nullptr; }
		if (SceneProxy) { Scene->RemovePrimitive(SceneProxy); SceneProxy = nullptr; }
	}
}

#include <cmath>
UGizmoComponent::UGizmoComponent()
{
	MeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
	LocalExtents = FVector(1.5f, 1.5f, 1.5f);
}

void UGizmoComponent::SetHolding(bool bHold)
{
	if (bIsHolding == bHold)
	{
		return;
	}

	UWorld* World = nullptr;
	if (Target)
	{
		World = Target->GetWorld();
	}
	if (!World && Owner)
	{
		World = Owner->GetWorld();
	}

	if (bHold)
	{
		if (World)
		{
			World->BeginDeferredPickingBVHUpdate();
		}
	}
	else if (World)
	{
		World->EndDeferredPickingBVHUpdate();
	}

	bIsHolding = bHold;
	if (bHold)
	{
		// Restart snap accumulation for each new gizmo drag so the first step is stable.
		ResetSnapAccumulation();
	}
}

bool UGizmoComponent::IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float AxisScale, float& OutRayT)
{
	FVector AxisStart = GetWorldLocation();
	FVector RayOrigin = Ray.Origin;
	FVector RayDirection = Ray.Direction;

	FVector AxisVector = AxisEnd - AxisStart;
	FVector DiffOrigin = RayOrigin - AxisStart;

	float RayDirDotRayDir = RayDirection.X * RayDirection.X + RayDirection.Y * RayDirection.Y + RayDirection.Z * RayDirection.Z;
	float RayDirDotAxis = RayDirection.X * AxisVector.X + RayDirection.Y * AxisVector.Y + RayDirection.Z * AxisVector.Z;
	float AxisDotAxis = AxisVector.X * AxisVector.X + AxisVector.Y * AxisVector.Y + AxisVector.Z * AxisVector.Z;
	float RayDirDotDiff = RayDirection.X * DiffOrigin.X + RayDirection.Y * DiffOrigin.Y + RayDirection.Z * DiffOrigin.Z;
	float AxisDotDiff = AxisVector.X * DiffOrigin.X + AxisVector.Y * DiffOrigin.Y + AxisVector.Z * DiffOrigin.Z;

	float Denominator = (RayDirDotRayDir * AxisDotAxis) - (RayDirDotAxis * RayDirDotAxis);

	float RayT;
	float AxisS;

	if (Denominator < 1e-6f)
	{
		RayT = 0.0f;
		AxisS = (AxisDotAxis > 0.0f) ? (AxisDotDiff / AxisDotAxis) : 0.0f;
	}
	else
	{
		RayT = (RayDirDotAxis * AxisDotDiff - AxisDotAxis * RayDirDotDiff) / Denominator;
		AxisS = (RayDirDotRayDir * AxisDotDiff - RayDirDotAxis * RayDirDotDiff) / Denominator;
	}

	if (RayT < 0.0f) RayT = 0.0f;

	if (AxisS < 0.0f) AxisS = 0.0f;
	else if (AxisS > 1.0f) AxisS = 1.0f;

	FVector ClosestPointOnRay = RayOrigin + (RayDirection * RayT);
	FVector ClosestPointOnAxis = AxisStart + (AxisVector * AxisS);

	FVector DistanceVector = ClosestPointOnRay - ClosestPointOnAxis;
	float DistanceSquared = (DistanceVector.X * DistanceVector.X) +
		(DistanceVector.Y * DistanceVector.Y) +
		(DistanceVector.Z * DistanceVector.Z);

	//기즈모 픽킹에 원기둥 크기를 반영합니다.
	float ClickThreshold = Radius * AxisScale;
	constexpr float StemRadius = 0.06f;
	ClickThreshold = StemRadius * AxisScale;
	float ClickThresholdSquared = ClickThreshold * ClickThreshold;

	if (DistanceSquared < ClickThresholdSquared)
	{
		OutRayT = RayT;
		return true;
	}

	return false;
}

bool UGizmoComponent::IntersectRayRotationHandle(const FRay& Ray, int32 Axis, float& OutRayT) const
{
	const FVector AxisVector = GetVectorForAxis(Axis).Normalized();
	const float Scale = (Axis == 0) ? GetWorldScale().X : (Axis == 1 ? GetWorldScale().Y : GetWorldScale().Z);
	const float RingRadius = AxisLength * Scale;
	const float RingThickness = Radius * Scale * 1.75f;

	const float Denom = Ray.Direction.Dot(AxisVector);
	if (std::abs(Denom) < 1e-6f)
	{
		return false;
	}

	const float RayT = (GetWorldLocation() - Ray.Origin).Dot(AxisVector) / Denom;
	if (RayT <= 0.0f)
	{
		return false;
	}

	const FVector HitPoint = Ray.Origin + Ray.Direction * RayT;
	const FVector Radial = HitPoint - GetWorldLocation();
	const FVector Planar = Radial - AxisVector * Radial.Dot(AxisVector);
	const float DistanceToRing = std::abs(Planar.Length() - RingRadius);
	if (DistanceToRing <= RingThickness)
	{
		OutRayT = RayT;
		return true;
	}

	return false;
}

void UGizmoComponent::HandleDrag(float DragAmount)
{
	// Snap is applied on the accumulated drag so mouse deltas do not jitter between steps.
	DragAmount = ApplySnapToDragAmount(DragAmount);
	if (DragAmount == 0.0f)
	{
		return;
	}

	switch (CurMode)
	{
	case EGizmoMode::Translate:
		TranslateTarget(DragAmount);
		break;
	case EGizmoMode::Rotate:
		RotateTarget(DragAmount);
		break;
	case EGizmoMode::Scale:
		ScaleTarget(DragAmount);
		break;
	default:
		break;
	}

	UpdateGizmoTransform();
}

float UGizmoComponent::ApplySnapToDragAmount(float DragAmount)
{
	bool bSnapEnabled = false;
	float SnapSize = 0.0f;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		bSnapEnabled = bTranslationSnapEnabled;
		SnapSize = TranslationSnapSize;
		break;
	case EGizmoMode::Rotate:
		bSnapEnabled = bRotationSnapEnabled;
		SnapSize = RotationSnapSizeRadians;
		break;
	case EGizmoMode::Scale:
		bSnapEnabled = bScaleSnapEnabled;
		SnapSize = ScaleSnapSize;
		break;
	default:
		break;
	}

	if (!bSnapEnabled || SnapSize <= FMath::Epsilon)
	{
		return DragAmount;
	}

	AccumulatedRawDragAmount += DragAmount;
	const float SnappedTotal = std::floor((AccumulatedRawDragAmount / SnapSize) + 0.5f) * SnapSize;
	const float DeltaToApply = SnappedTotal - LastAppliedSnappedDragAmount;
	LastAppliedSnappedDragAmount = SnappedTotal;
	return DeltaToApply;
}

void UGizmoComponent::ResetSnapAccumulation()
{
	AccumulatedRawDragAmount = 0.0f;
	LastAppliedSnappedDragAmount = 0.0f;
}

void UGizmoComponent::TranslateTarget(float DragAmount)
{
	if (!Target) return;

	FVector Delta = GetVectorForAxis(SelectedAxis) * DragAmount;
	AddWorldOffset(Delta);
	Target->AddWorldOffset(Delta);
}

void UGizmoComponent::RotateTarget(float DragAmount)
{
	if (!Target || !Target->IsValid()) return;

	FVector Axis = GetVectorForAxis(SelectedAxis);
	FQuat DeltaQuat = FQuat::FromAxisAngle(Axis, DragAmount);
	Target->AddWorldRotation(DeltaQuat, bIsWorldSpace);
}

void UGizmoComponent::ScaleTarget(float DragAmount)
{
	if (!Target || !Target->IsValid()) return;

	FVector Delta = FVector::ZeroVector;
	const float ScaleDelta = DragAmount * ScaleSensitivity;

	if (SelectedAxis == 0) Delta.X = ScaleDelta;
	if (SelectedAxis == 1) Delta.Y = ScaleDelta;
	if (SelectedAxis == 2) Delta.Z = ScaleDelta;

	Target->AddScaleDelta(Delta);
}

bool UGizmoComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	OutHitResult = {};
	if (!MeshData || MeshData->Indices.empty())
	{
		return false;
	}

	float BestRayT = FLT_MAX;
	int32 BestAxis = -1;
	const FVector GizmoLocation = GetWorldLocation();

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if ((AxisMask & (1u << Axis)) == 0)
		{
			continue;
		}

		float RayT = 0.0f;
		bool bAxisHit = false;
		if (CurMode == EGizmoMode::Rotate)
		{
			bAxisHit = IntersectRayRotationHandle(Ray, Axis, RayT);
		}
		else
		{
			const FVector AxisDir = GetVectorForAxis(Axis).Normalized();
			const float AxisScale = (Axis == 0) ? GetWorldScale().X : (Axis == 1 ? GetWorldScale().Y : GetWorldScale().Z);
			const FVector AxisEnd = GizmoLocation + AxisDir * AxisLength * AxisScale;
			bAxisHit = IntersectRayAxis(Ray, AxisEnd, AxisScale, RayT);
		}

		if (bAxisHit && RayT < BestRayT)
		{
			BestRayT = RayT;
			BestAxis = Axis;
		}
	}

	if (BestAxis >= 0)
	{
		OutHitResult.bHit = true;
		OutHitResult.Distance = BestRayT;
		OutHitResult.HitComponent = this;
		if (!IsHolding())
		{
			SelectedAxis = BestAxis;
		}
		return true;
	}

	if (!IsHolding())
	{
		SelectedAxis = -1;
	}
	return false;
}


FVector UGizmoComponent::GetVectorForAxis(int32 Axis) const
{
	switch (Axis)
	{
	case 0:
		return GetForwardVector();
	case 1:
		return GetRightVector();
	case 2:
		return GetUpVector();
	default:
		return FVector(0.f, 0.f, 0.f);
	}
}

void UGizmoComponent::SetTarget(IGizmoTransformTarget* NewTarget)
{
	Target = NewTarget;

	if (!Target || !Target->IsValid())
	{
		Deactivate();
		return;
	}

	SetWorldLocation(Target->GetWorldLocation());
	UpdateGizmoTransform();
	SetVisibility(true);
}

void UGizmoComponent::SetTarget(USceneComponent* NewTarget)
{
	if (!NewTarget)
	{
		Deactivate();
		return;
	}

	ComponentTarget.SetComponent(NewTarget);
	SetTarget(&ComponentTarget);
}

void UGizmoComponent::SetTarget(AActor* NewTarget)
{
	SetTarget(NewTarget ? NewTarget->GetRootComponent() : nullptr);
}

void UGizmoComponent::UpdateLinearDrag(const FRay& Ray)
{
	FVector AxisVector = GetVectorForAxis(SelectedAxis);

	FVector PlaneNormal = AxisVector.Cross(Ray.Direction);
	FVector ProjectDir = PlaneNormal.Cross(AxisVector);

	float Denom = Ray.Direction.Dot(ProjectDir);
	if (std::abs(Denom) < 1e-6f) return;

	float DistanceToPlane = (GetWorldLocation() - Ray.Origin).Dot(ProjectDir) / Denom;
	FVector CurrentIntersectionLocation = Ray.Origin + (Ray.Direction * DistanceToPlane);

	if (bIsFirstFrameOfDrag)
	{
		LastIntersectionLocation = CurrentIntersectionLocation;
		bIsFirstFrameOfDrag = false;
		return;
	}

	FVector FullDelta = CurrentIntersectionLocation - LastIntersectionLocation;

	float DragAmount = FullDelta.Dot(AxisVector);

	HandleDrag(DragAmount);

	LastIntersectionLocation = CurrentIntersectionLocation;
}

void UGizmoComponent::UpdateAngularDrag(const FRay& Ray)
{
	FVector AxisVector = GetVectorForAxis(SelectedAxis);
	FVector PlaneNormal = AxisVector;

	float Denom = Ray.Direction.Dot(PlaneNormal);
	if (std::abs(Denom) < 1e-6f) return;

	float DistanceToPlane = (GetWorldLocation() - Ray.Origin).Dot(PlaneNormal) / Denom;
	FVector CurrentIntersectionLocation = Ray.Origin + (Ray.Direction * DistanceToPlane);

	if (bIsFirstFrameOfDrag)
	{
		LastIntersectionLocation = CurrentIntersectionLocation;
		bIsFirstFrameOfDrag = false;
		return;
	}

	FVector CenterToLast = (LastIntersectionLocation - GetWorldLocation()).Normalized();
	FVector CenterToCurrent = (CurrentIntersectionLocation - GetWorldLocation()).Normalized();

	float DotProduct = Clamp(CenterToLast.Dot(CenterToCurrent), -1.0f, 1.0f);
	float AngleRadians = std::acos(DotProduct);

	FVector CrossProduct = CenterToLast.Cross(CenterToCurrent);
	float Sign = (CrossProduct.Dot(AxisVector) >= 0.0f) ? 1.0f : -1.0f;

	float DeltaAngle = Sign * AngleRadians;

	HandleDrag(DeltaAngle);

	LastIntersectionLocation = CurrentIntersectionLocation;
}

void UGizmoComponent::UpdateHoveredAxis(int Index)
{
	if (Index < 0)
	{
		if (IsHolding() == false) SelectedAxis = -1;
	}
	else
	{
		if (IsHolding() == false)
		{
			uint32 VertexIndex = MeshData->Indices[Index];
			uint32 HitAxis = MeshData->Vertices[VertexIndex].SubID;

			// 마스크에 의해 숨겨진 축은 선택 불가
			if (AxisMask & (1u << HitAxis))
			{
				SelectedAxis = HitAxis;
			}
			else
			{
				SelectedAxis = -1;
			}
		}
	}
}

void UGizmoComponent::UpdateDrag(const FRay& Ray)
{
	if (!IsHolding() || !IsActive()) return;
	if (SelectedAxis == -1 || !Target || !Target->IsValid()) return;

	CurMode == EGizmoMode::Rotate ? UpdateAngularDrag(Ray) : UpdateLinearDrag(Ray);
}

void UGizmoComponent::DragEnd()
{
	bIsFirstFrameOfDrag = true;
	// Clear leftover snap state so the next drag starts from zero.
	ResetSnapAccumulation();
	SetHolding(false);
	SetPressedOnHandle(false);
}

void UGizmoComponent::SetNextMode()
{
	EGizmoMode NextMode = static_cast<EGizmoMode>((static_cast<int>(CurMode) + 1) % EGizmoMode::End);
	UpdateGizmoMode(NextMode);
}

void UGizmoComponent::UpdateGizmoMode(EGizmoMode NewMode)
{
	CurMode = NewMode;
	UpdateGizmoTransform();
}

void UGizmoComponent::UpdateGizmoTransform()
{
	if (!Target) return;

	SetWorldLocation(Target->GetWorldLocation());

	if (CurMode == EGizmoMode::Scale || !bIsWorldSpace)
	{
		SetRelativeRotation(Target->GetWorldRotation());
	}

	const FMeshData* DesiredMeshData = nullptr;

	switch (CurMode)
	{
	case EGizmoMode::Scale:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::ScaleGizmo);
		break;

	case EGizmoMode::Rotate:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::RotGizmo);
		break;

	case EGizmoMode::Translate:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
		break;

	default:
		break;
	}

	if (FVector::DistSquared(GetWorldLocation(), Target->GetWorldLocation()) > FMath::Epsilon * FMath::Epsilon)
	{
		SetWorldLocation(Target->GetWorldLocation());
	}

	if (MeshData != DesiredMeshData && DesiredMeshData)
	{
		MeshData = DesiredMeshData;
		MarkRenderStateDirty();
	}
}

float UGizmoComponent::ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth) const
{
	float NewScale;
	if (bIsOrtho)
	{
		NewScale = OrthoWidth * GizmoScreenScale;
	}
	else
	{
		float Distance = FVector::Distance(CameraLocation, GetWorldLocation());
		NewScale = Distance * GizmoScreenScale;
	}
	return (NewScale < 0.01f) ? 0.01f : NewScale;
}

void UGizmoComponent::ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth)
{
	float NewScale = ComputeScreenSpaceScale(CameraLocation, bIsOrtho, OrthoWidth);
	SetRelativeScale(FVector(NewScale, NewScale, NewScale));
}

void UGizmoComponent::SetWorldSpace(bool bWorldSpace)
{
	bIsWorldSpace = bWorldSpace;
	UpdateGizmoTransform();
}

void UGizmoComponent::SetSnapSettings(bool bTranslationEnabled, float InTranslationSnapSize,
	bool bRotationEnabled, float InRotationSnapSizeDegrees,
	bool bScaleEnabled, float InScaleSnapSize)
{
	bTranslationSnapEnabled = bTranslationEnabled;
	TranslationSnapSize = (InTranslationSnapSize > FMath::Epsilon) ? InTranslationSnapSize : 10.0f;
	bRotationSnapEnabled = bRotationEnabled;
	RotationSnapSizeRadians = ((InRotationSnapSizeDegrees > FMath::Epsilon) ? InRotationSnapSizeDegrees : 15.0f) * DEG_TO_RAD;
	bScaleSnapEnabled = bScaleEnabled;
	ScaleSnapSize = (InScaleSnapSize > FMath::Epsilon) ? InScaleSnapSize : 0.1f;
}

uint32 UGizmoComponent::ComputeAxisMask(ELevelViewportType ViewportType, EGizmoMode Mode)
{
	constexpr uint32 AllAxes = 0x7;
	uint32 ViewAxis = AllAxes;

	switch (ViewportType)
	{
	case ELevelViewportType::Top:
	case ELevelViewportType::Bottom:
		ViewAxis = 0x4; break; // Z
	case ELevelViewportType::Front:
	case ELevelViewportType::Back:
		ViewAxis = 0x1; break; // X
	case ELevelViewportType::Left:
	case ELevelViewportType::Right:
		ViewAxis = 0x2; break; // Y
	default: break;
	}

	if (ViewAxis == AllAxes)
		return AllAxes;

	if (Mode == EGizmoMode::Rotate)
		return ViewAxis;            // Rotate: 시선 축만

	return AllAxes & ~ViewAxis;     // Translate/Scale: 시선 축 제외
}

void UGizmoComponent::Deactivate()
{
	if (bIsHolding)
	{
		SetHolding(false);
	}

	Target = nullptr;
	ComponentTarget.SetComponent(nullptr);
	AllSelectedActors = nullptr;
	SetVisibility(false);
	SelectedAxis = -1;
}

FMeshBuffer* UGizmoComponent::GetMeshBuffer() const
{
	EMeshShape Shape = EMeshShape::TransGizmo;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		break;
	case EGizmoMode::Rotate:
		Shape = EMeshShape::RotGizmo;
		break;
	case EGizmoMode::Scale:
		Shape = EMeshShape::ScaleGizmo;
		break;
	}
	return &FMeshBufferManager::Get().GetMeshBuffer(Shape);
}
