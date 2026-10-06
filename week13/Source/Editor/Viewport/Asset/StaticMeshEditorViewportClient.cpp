#include "StaticMeshEditorViewportClient.h"

#include "Collision/Ray/RayUtils.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Gizmo/GizmoTransformTarget.h"
#include "Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Mesh/Static/StaticMesh.h"
#include "Object/Reflection/ObjectFactory.h"
#include "PhysicsEngine/BodySetup.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include <cmath>
#include <cfloat>
#include <imgui.h>
#include <utility>

namespace
{
	constexpr float MinShapeSize = 0.1f;

	struct FLocalRay
	{
		FVector Origin = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
	};

	FVector SafeDivide(const FVector& Value, const FVector& Divisor)
	{
		return FVector(
			FMath::Abs(Divisor.X) > FMath::KINDA_SMALL_NUMBER ? Value.X / Divisor.X : Value.X,
			FMath::Abs(Divisor.Y) > FMath::KINDA_SMALL_NUMBER ? Value.Y / Divisor.Y : Value.Y,
			FMath::Abs(Divisor.Z) > FMath::KINDA_SMALL_NUMBER ? Value.Z / Divisor.Z : Value.Z);
	}

	float GetSafeUniformScale(const FVector& Scale3D)
	{
		const float UniformScale = Scale3D.GetAbsMax();
		return FMath::Abs(UniformScale) > FMath::KINDA_SMALL_NUMBER ? UniformScale : 1.0f;
	}

	FTransform GetLocalTransformFromWorld(const FTransform& WorldTM, const FTransform& ParentTM)
	{
		return FTransform::FromMatrixWithScale(WorldTM.ToMatrix() * ParentTM.ToMatrix().GetAffineInverse());
	}

	UBodySetup* GetStaticMeshBodySetup(UStaticMeshComponent* Component)
	{
		UStaticMesh* StaticMesh = Component ? Component->GetStaticMesh() : nullptr;
		return StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	}

	FKShapeElem* GetBodySetupShape(UBodySetup* BodySetup, FBodySetupShapeSelection Selection)
	{
		return BodySetup && Selection.IsValid()
			? BodySetup->GetAggGeom().GetElement(Selection.Type, Selection.Index)
			: nullptr;
	}

	const FKShapeElem* GetBodySetupShape(const UBodySetup* BodySetup, FBodySetupShapeSelection Selection)
	{
		return BodySetup && Selection.IsValid()
			? BodySetup->GetAggGeom().GetElement(Selection.Type, Selection.Index)
			: nullptr;
	}

	FRay MakeNormalizedRay(const FRay& Ray)
	{
		FRay NormalizedRay = Ray;
		NormalizedRay.Direction = Ray.Direction.GetSafeNormal();
		return NormalizedRay;
	}

	FLocalRay MakeLocalRay(const FRay& WorldRay, const FTransform& LocalToWorld)
	{
		const FMatrix WorldToLocal = LocalToWorld.ToMatrix().GetAffineInverse();
		FLocalRay LocalRay;
		LocalRay.Origin = WorldToLocal.TransformPositionWithW(WorldRay.Origin);
		LocalRay.Direction = WorldToLocal.TransformVector(WorldRay.Direction).GetSafeNormal();
		return LocalRay;
	}

	bool ShouldUseHit(float Distance, float CurrentBestDistance)
	{
		return Distance >= 0.0f && Distance < CurrentBestDistance;
	}

	bool IntersectSphere(const FRay& Ray, const FVector& Center, float Radius, float& OutDistance)
	{
		if (Radius <= 0.0f)
		{
			return false;
		}

		const FVector M = Ray.Origin - Center;
		const float B = M.Dot(Ray.Direction);
		const float C = M.Dot(M) - Radius * Radius;
		const float Discriminant = B * B - C;
		if (Discriminant < 0.0f)
		{
			return false;
		}

		const float Root = std::sqrt(Discriminant);
		OutDistance = -B - Root;
		if (OutDistance < 0.0f)
		{
			OutDistance = -B + Root;
		}
		return OutDistance >= 0.0f;
	}

	bool IntersectBox(const FRay& Ray, const FTransform& BoxWorldTM, const FVector& HalfExtent, float& OutDistance)
	{
		if (HalfExtent.X <= 0.0f || HalfExtent.Y <= 0.0f || HalfExtent.Z <= 0.0f)
		{
			return false;
		}

		const FLocalRay LocalRay = MakeLocalRay(Ray, BoxWorldTM);
		FRay LocalRayForAABB { LocalRay.Origin, LocalRay.Direction };
		float TMin = 0.0f;
		float TMax = 0.0f;
		if (!FRayUtils::IntersectRayAABB(LocalRayForAABB, -HalfExtent, HalfExtent, TMin, TMax))
		{
			return false;
		}

		const float LocalDistance = TMin >= 0.0f ? TMin : TMax;
		if (LocalDistance < 0.0f)
		{
			return false;
		}

		const FVector WorldHit = BoxWorldTM.TransformPosition(LocalRay.Origin + LocalRay.Direction * LocalDistance);
		OutDistance = FVector::Distance(Ray.Origin, WorldHit);
		return true;
	}

	bool IntersectLocalSphere(const FLocalRay& LocalRay, const FVector& Center, float Radius, float& InOutBestDistance)
	{
		FRay SphereRay { LocalRay.Origin, LocalRay.Direction };
		float Distance = 0.0f;
		if (IntersectSphere(SphereRay, Center, Radius, Distance) && Distance < InOutBestDistance)
		{
			InOutBestDistance = Distance;
			return true;
		}
		return false;
	}

	bool IntersectCapsule(const FRay& Ray, const FTransform& CapsuleWorldTM, float Radius, float Length, float& OutDistance)
	{
		if (Radius <= 0.0f || Length < 0.0f)
		{
			return false;
		}

		const FLocalRay LocalRay = MakeLocalRay(Ray, CapsuleWorldTM);
		const float HalfLength = Length * 0.5f;
		const FVector P = LocalRay.Origin;
		const FVector D = LocalRay.Direction;
		const float A = D.X * D.X + D.Y * D.Y;
		float BestLocalDistance = FLT_MAX;
		bool bHit = false;

		if (A > FMath::KINDA_SMALL_NUMBER)
		{
			const float B = P.X * D.X + P.Y * D.Y;
			const float C = P.X * P.X + P.Y * P.Y - Radius * Radius;
			const float Discriminant = B * B - A * C;
			if (Discriminant >= 0.0f)
			{
				const float Root = std::sqrt(Discriminant);
				const float Candidates[2] = { (-B - Root) / A, (-B + Root) / A };
				for (float Candidate : Candidates)
				{
					if (Candidate < 0.0f || Candidate >= BestLocalDistance)
					{
						continue;
					}

					const float Z = P.Z + D.Z * Candidate;
					if (Z >= -HalfLength && Z <= HalfLength)
					{
						BestLocalDistance = Candidate;
						bHit = true;
					}
				}
			}
		}

		bHit = IntersectLocalSphere(LocalRay, FVector(0.0f, 0.0f, HalfLength), Radius, BestLocalDistance) || bHit;
		bHit = IntersectLocalSphere(LocalRay, FVector(0.0f, 0.0f, -HalfLength), Radius, BestLocalDistance) || bHit;
		if (!bHit)
		{
			return false;
		}

		const FVector WorldHit = CapsuleWorldTM.TransformPosition(LocalRay.Origin + LocalRay.Direction * BestLocalDistance);
		OutDistance = FVector::Distance(Ray.Origin, WorldHit);
		return true;
	}

	bool IntersectConvex(const FRay& Ray, const FKConvexElem& ConvexElem, const FTransform& ConvexWorldTM, float& OutDistance)
	{
		bool bHit = false;
		OutDistance = FLT_MAX;
		for (std::size_t Index = 0; Index + 2 < ConvexElem.IndexData.size(); Index += 3)
		{
			const int32 I0 = ConvexElem.IndexData[Index];
			const int32 I1 = ConvexElem.IndexData[Index + 1];
			const int32 I2 = ConvexElem.IndexData[Index + 2];
			if (I0 < 0 || I1 < 0 || I2 < 0 ||
				static_cast<std::size_t>(I0) >= ConvexElem.VertexData.size() ||
				static_cast<std::size_t>(I1) >= ConvexElem.VertexData.size() ||
				static_cast<std::size_t>(I2) >= ConvexElem.VertexData.size())
			{
				continue;
			}

			const FVector V0 = ConvexWorldTM.TransformPosition(ConvexElem.VertexData[I0]);
			const FVector V1 = ConvexWorldTM.TransformPosition(ConvexElem.VertexData[I1]);
			const FVector V2 = ConvexWorldTM.TransformPosition(ConvexElem.VertexData[I2]);
			float Distance = 0.0f;
			if (FRayUtils::IntersectTriangle(Ray.Origin, Ray.Direction, V0, V1, V2, Distance) &&
				Distance < OutDistance)
			{
				OutDistance = Distance;
				bHit = true;
			}
		}
		return bHit;
	}
}

struct FBodySetupShapeGizmoTarget::FEditableShape
{
	EAggCollisionShape Type = EAggCollisionShape::Unknown;
	FKShapeElem* Shape = nullptr;
};

void FBodySetupShapeGizmoTarget::SetShape(UStaticMeshComponent* InComponent, FBodySetupShapeSelection InSelection)
{
	Component = InComponent;
	Selection = InSelection;
}

void FBodySetupShapeGizmoTarget::Clear()
{
	Component = nullptr;
	Selection = {};
}

bool FBodySetupShapeGizmoTarget::IsValid() const
{
	FEditableShape Shape;
	return GetEditableShape(Shape);
}

UWorld* FBodySetupShapeGizmoTarget::GetWorld() const
{
	UStaticMeshComponent* CurrentComponent = GetComponent();
	return CurrentComponent ? CurrentComponent->GetWorld() : nullptr;
}

FVector FBodySetupShapeGizmoTarget::GetWorldLocation() const
{
	return GetShapeWorldTransform().Location;
}

FRotator FBodySetupShapeGizmoTarget::GetWorldRotation() const
{
	return GetShapeWorldTransform().GetRotator();
}

FQuat FBodySetupShapeGizmoTarget::GetWorldQuat() const
{
	return GetShapeWorldTransform().Rotation;
}

FVector FBodySetupShapeGizmoTarget::GetWorldScale() const
{
	return GetShapeWorldTransform().Scale;
}

void FBodySetupShapeGizmoTarget::SetWorldLocation(const FVector& NewLocation)
{
	FEditableShape Shape;
	FTransform ComponentTM;
	FVector Scale3D;
	if (!GetEditableShape(Shape) || !GetComponentEditTransform(ComponentTM, Scale3D))
	{
		return;
	}

	const FVector LocalLocation = SafeDivide(ComponentTM.InverseTransformPositionNoScale(NewLocation), Scale3D);
	switch (Shape.Type)
	{
	case EAggCollisionShape::Sphere:
		static_cast<FKSphereElem*>(Shape.Shape)->Center = LocalLocation;
		break;
	case EAggCollisionShape::Box:
		static_cast<FKBoxElem*>(Shape.Shape)->Center = LocalLocation;
		break;
	case EAggCollisionShape::Sphyl:
		static_cast<FKSphylElem*>(Shape.Shape)->Center = LocalLocation;
		break;
	case EAggCollisionShape::Convex:
	{
		FKConvexElem* ConvexElem = static_cast<FKConvexElem*>(Shape.Shape);
		FTransform LocalTM = ConvexElem->GetTransform();
		LocalTM.Location = LocalLocation;
		ConvexElem->SetTransform(LocalTM);
		break;
	}
	default:
		return;
	}

	MarkShapeChanged();
}

void FBodySetupShapeGizmoTarget::SetWorldRotation(const FRotator& NewRotation)
{
	SetWorldRotation(NewRotation.ToQuaternion());
}

void FBodySetupShapeGizmoTarget::SetWorldRotation(const FQuat& NewQuat)
{
	FEditableShape Shape;
	FTransform ComponentTM;
	FVector Scale3D;
	if (!GetEditableShape(Shape) || !GetComponentEditTransform(ComponentTM, Scale3D))
	{
		return;
	}

	if (Shape.Type == EAggCollisionShape::Sphere)
	{
		return;
	}

	const FTransform CurrentWorldTM = GetShapeWorldTransform();
	const FTransform DesiredWorldTM(CurrentWorldTM.Location, NewQuat, CurrentWorldTM.Scale);
	FTransform LocalTM = GetLocalTransformFromWorld(DesiredWorldTM, ComponentTM);

	switch (Shape.Type)
	{
	case EAggCollisionShape::Box:
		static_cast<FKBoxElem*>(Shape.Shape)->Center = SafeDivide(LocalTM.Location, Scale3D);
		static_cast<FKBoxElem*>(Shape.Shape)->Rotation = LocalTM.GetRotator();
		break;
	case EAggCollisionShape::Sphyl:
		static_cast<FKSphylElem*>(Shape.Shape)->Center = SafeDivide(LocalTM.Location, Scale3D);
		static_cast<FKSphylElem*>(Shape.Shape)->Rotation = LocalTM.GetRotator();
		break;
	case EAggCollisionShape::Convex:
		LocalTM.Location = SafeDivide(LocalTM.Location, Scale3D);
		static_cast<FKConvexElem*>(Shape.Shape)->SetTransform(LocalTM);
		break;
	default:
		return;
	}

	MarkShapeChanged();
}

void FBodySetupShapeGizmoTarget::SetWorldScale(const FVector& NewScale)
{
	AddScaleDelta(NewScale - GetWorldScale());
}

void FBodySetupShapeGizmoTarget::AddWorldOffset(const FVector& Delta)
{
	SetWorldLocation(GetWorldLocation() + Delta);
}

void FBodySetupShapeGizmoTarget::AddWorldRotation(const FQuat& Delta, bool bWorldSpace)
{
	if (!IsValid())
	{
		return;
	}

	const FQuat CurrentQuat = GetWorldQuat();
	SetWorldRotation(bWorldSpace ? Delta * CurrentQuat : CurrentQuat * Delta);
}

void FBodySetupShapeGizmoTarget::AddScaleDelta(const FVector& Delta)
{
	FEditableShape Shape;
	FTransform ComponentTM;
	FVector Scale3D;
	if (!GetEditableShape(Shape) || !GetComponentEditTransform(ComponentTM, Scale3D))
	{
		return;
	}

	const FVector LocalDelta = SafeDivide(Delta, Scale3D);
	switch (Shape.Type)
	{
	case EAggCollisionShape::Sphere:
		static_cast<FKSphereElem*>(Shape.Shape)->ScaleElem(LocalDelta, MinShapeSize);
		break;
	case EAggCollisionShape::Box:
		static_cast<FKBoxElem*>(Shape.Shape)->ScaleElem(LocalDelta, MinShapeSize);
		break;
	case EAggCollisionShape::Sphyl:
		static_cast<FKSphylElem*>(Shape.Shape)->ScaleElem(LocalDelta, MinShapeSize);
		break;
	case EAggCollisionShape::Convex:
		static_cast<FKConvexElem*>(Shape.Shape)->ScaleElem(LocalDelta, MinShapeSize);
		static_cast<FKConvexElem*>(Shape.Shape)->UpdateElemBox();
		break;
	default:
		return;
	}

	MarkShapeChanged();
}

UStaticMeshComponent* FBodySetupShapeGizmoTarget::GetComponent() const
{
	return Component.Get();
}

bool FBodySetupShapeGizmoTarget::GetEditableShape(FEditableShape& OutShape) const
{
	if (FKShapeElem* Shape = GetBodySetupShape(GetStaticMeshBodySetup(GetComponent()), Selection))
	{
		OutShape = { Selection.Type, Shape };
		return true;
	}

	return false;
}

bool FBodySetupShapeGizmoTarget::GetComponentEditTransform(FTransform& OutComponentTM, FVector& OutScale3D) const
{
	UStaticMeshComponent* CurrentComponent = GetComponent();
	if (!CurrentComponent)
	{
		return false;
	}

	OutComponentTM = FTransform::FromMatrixWithScale(CurrentComponent->GetWorldMatrix());
	OutScale3D = OutComponentTM.Scale;
	OutComponentTM.Scale = FVector::OneVector;
	return true;
}

FTransform FBodySetupShapeGizmoTarget::GetShapeWorldTransform() const
{
	FEditableShape Shape;
	FTransform ComponentTM;
	FVector Scale3D;
	if (!GetEditableShape(Shape) || !GetComponentEditTransform(ComponentTM, Scale3D))
	{
		return FTransform();
	}

	const float UniformScale = GetSafeUniformScale(Scale3D);
	switch (Shape.Type)
	{
	case EAggCollisionShape::Sphere:
	{
		const FKSphereElem* SphereElem = static_cast<const FKSphereElem*>(Shape.Shape);
		const FVector WorldCenter = ComponentTM.TransformPosition(SphereElem->Center * UniformScale);
		const float Diameter = FMath::Max(SphereElem->Radius * 2.0f * UniformScale, MinShapeSize);
		return FTransform(WorldCenter, ComponentTM.Rotation, FVector(Diameter, Diameter, Diameter));
	}
	case EAggCollisionShape::Box:
	{
		const FKBoxElem* BoxElem = static_cast<const FKBoxElem*>(Shape.Shape);
		FTransform ShapeWorldTM = FTransform(BoxElem->Center * Scale3D, BoxElem->Rotation) * ComponentTM;
		ShapeWorldTM.Scale = FVector(
			FMath::Max(BoxElem->X * FMath::Abs(Scale3D.X), MinShapeSize),
			FMath::Max(BoxElem->Y * FMath::Abs(Scale3D.Y), MinShapeSize),
			FMath::Max(BoxElem->Z * FMath::Abs(Scale3D.Z), MinShapeSize));
		return ShapeWorldTM;
	}
	case EAggCollisionShape::Sphyl:
	{
		const FKSphylElem* SphylElem = static_cast<const FKSphylElem*>(Shape.Shape);
		FTransform ShapeWorldTM = FTransform(SphylElem->Center * UniformScale, SphylElem->Rotation) * ComponentTM;
		const float Diameter = FMath::Max(SphylElem->Radius * 2.0f * UniformScale, MinShapeSize);
		ShapeWorldTM.Scale = FVector(Diameter, Diameter, FMath::Max((SphylElem->Length + SphylElem->Radius * 2.0f) * UniformScale, MinShapeSize));
		return ShapeWorldTM;
	}
	case EAggCollisionShape::Convex:
	{
		const FKConvexElem* ConvexElem = static_cast<const FKConvexElem*>(Shape.Shape);
		FTransform ShapeWorldTM = ConvexElem->GetTransform() * ComponentTM;
		ShapeWorldTM.Scale *= UniformScale;
		return ShapeWorldTM;
	}
	default:
		return FTransform();
	}
}

void FBodySetupShapeGizmoTarget::MarkShapeChanged() const
{
	// BodySetup debug lines are generated from the asset each frame. Recreating the
	// StaticMesh render proxy here makes the preview mesh flicker or disappear while dragging.
}

void FStaticMeshEditorViewportClient::Initialize(ID3D11Device* Device, uint32 Width, uint32 Height)
{
	Viewport = new FViewport();
	Viewport->Initialize(Device, Width, Height);
	Viewport->SetClient(this);

	bIsRenderable = true;
}


void FStaticMeshEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PreviewWorld);
	Collector.AddReferencedObject(PreviewActor);
	Collector.AddReferencedObject(PreviewMeshComponent);
	Collector.AddReferencedObject(Gizmo);
}

void FStaticMeshEditorViewportClient::Release()
{
	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	PreviewWorld = nullptr;
	PreviewActor = nullptr;
	PreviewMeshComponent = nullptr;
	UObjectManager::Get().DestroyObject(Gizmo);
	Gizmo = nullptr;
	BodySetupShapeTarget.Clear();
	bBodySetupEditingEnabled = false;
	SelectedBodySetupShape = {};
	OnBodySetupShapePicked = nullptr;
	OnBodySetupShapeEdited = nullptr;
	bIsRenderable = false;
}

void FStaticMeshEditorViewportClient::CreatePreviewGizmo()
{
	if (!PreviewWorld)
	{
		return;
	}

	Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	Gizmo->SetScene(&PreviewWorld->GetScene());
	Gizmo->CreateRenderState();
	Gizmo->Deactivate();
}

void FStaticMeshEditorViewportClient::ResetCameraToPreviewBounds()
{
	FBoundingBox Bounds = PreviewMeshComponent
		? PreviewMeshComponent->GetWorldBoundingBox()
		: FBoundingBox(FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f));

	FVector Center = Bounds.GetCenter();
	float Radius = Bounds.GetExtent().Length();
	if (Radius < 0.1f)
	{
		Radius = 1.0f;
	}

	const float FovRadians = ViewTransform.FOV;
	const float Distance = Radius / std::tan(FovRadians * 0.5f) * 1.25f;
	const FVector ViewDir = FVector(-1.0f, -1.0f, -0.6f).Normalized();

	ViewTransform.ViewLocation = Center - ViewDir * Distance;
	ViewTransform.LookAt(Center);

	TargetLocation = ViewTransform.ViewLocation;
	LastAppliedCameraLocation = ViewTransform.ViewLocation;
	bTargetLocationInitialized = true;
	bLastAppliedCameraLocationInitialized = true;
}

bool FStaticMeshEditorViewportClient::IsMouseOverViewport() const
{
	if (!bIsRenderable || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f) return false;

	ImVec2 MousePos = ImGui::GetMousePos();
	return MousePos.x >= ViewportScreenRect.X && MousePos.x <= (ViewportScreenRect.X + ViewportScreenRect.Width) &&
		MousePos.y >= ViewportScreenRect.Y && MousePos.y <= (ViewportScreenRect.Y + ViewportScreenRect.Height);
}

void FStaticMeshEditorViewportClient::NotifyViewportResized(int32 NewWidth, int32 NewHeight)
{
	if (Viewport && NewHeight > 0)
	{
		ViewTransform.AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
	}
}

bool FStaticMeshEditorViewportClient::GetCameraView(FMinimalViewInfo& OutPOV) const
{
	OutPOV.Location = ViewTransform.ViewLocation;
	OutPOV.Rotation = ViewTransform.ViewRotation;
	OutPOV.FOV = ViewTransform.FOV;
	OutPOV.AspectRatio = ViewTransform.AspectRatio;
	return true;
}

void FStaticMeshEditorViewportClient::Tick(float DeltaTime)
{
	SyncCameraSmoothingTarget();
	ApplySmoothedCameraLocation(DeltaTime);
	TickShortcuts();
	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
}

void FStaticMeshEditorViewportClient::ApplyTransformSettingsToGizmo()
{
	if (!Gizmo)
	{
		return;
	}

	const FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
	const bool bForceLocalForScale = Gizmo->GetMode() == EGizmoMode::Scale;
	Gizmo->SetWorldSpace(bForceLocalForScale ? false : Settings.CoordSystem == EEditorCoordSystem::World);
	Gizmo->SetSnapSettings(
		Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
		Settings.bEnableRotationSnap, Settings.RotationSnapSize,
		Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
}

void FStaticMeshEditorViewportClient::SetBodySetupEditingEnabled(bool bInEnabled)
{
	bBodySetupEditingEnabled = bInEnabled;
	if (!bBodySetupEditingEnabled)
	{
		SelectedBodySetupShape = {};
		BodySetupShapeTarget.Clear();
		if (Gizmo && IsBodySetupShapeGizmoActive())
		{
			Gizmo->Deactivate();
		}
	}
	else
	{
		SyncBodySetupShapeGizmoTarget();
	}
}

void FStaticMeshEditorViewportClient::SetSelectedBodySetupShape(FBodySetupShapeSelection InSelection)
{
	SelectedBodySetupShape = InSelection;
	SyncBodySetupShapeGizmoTarget();
}

void FStaticMeshEditorViewportClient::MarkBodySetupDebugDirty()
{
	++BodySetupDebugRevision;
	SyncBodySetupShapeGizmoTarget();
}

void FStaticMeshEditorViewportClient::SetOnBodySetupShapePicked(TFunction<void(FBodySetupShapeSelection)> InCallback)
{
	OnBodySetupShapePicked = std::move(InCallback);
}

void FStaticMeshEditorViewportClient::SetOnBodySetupShapeEdited(TFunction<void()> InCallback)
{
	OnBodySetupShapeEdited = std::move(InCallback);
}

void FStaticMeshEditorViewportClient::TickShortcuts()
{
	if (!FSlateApplication::Get().DoesClientOwnKeyboardInput(this)) return;

	if (InputSystem::Get().GetKeyDown('F'))
	{
		ResetCameraToPreviewBounds();
	}
}

void FStaticMeshEditorViewportClient::TickInput(float DeltaTime)
{
	if (!FSlateApplication::Get().DoesClientOwnMouseInput(this)) return;
	// 텍스트 입력 중에는 카메라 키/마우스 조작을 가로채지 않는다.
	if (ImGui::GetIO().WantTextInput) return;

	FViewportCameraControlSettings& ControlSettings = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls;
	InputSystem& Input = InputSystem::Get();

	FVector LocalMove = FVector::ZeroVector;
	float WorldVerticalMove = 0.0f;
	const float CameraSpeed = ControlSettings.MoveSpeed;

	if (Input.GetKey('W')) LocalMove.X += CameraSpeed;
	if (Input.GetKey('S')) LocalMove.X -= CameraSpeed;
	if (Input.GetKey('D')) LocalMove.Y += CameraSpeed;
	if (Input.GetKey('A')) LocalMove.Y -= CameraSpeed;
	if (Input.GetKey('Q')) WorldVerticalMove -= CameraSpeed;
	if (Input.GetKey('E')) WorldVerticalMove += CameraSpeed;

	const FVector Forward = ViewTransform.ViewRotation.GetForwardVector();
	const FVector Right = ViewTransform.ViewRotation.GetRightVector();

	FVector DeltaMove = (Forward * LocalMove.X + Right * LocalMove.Y) * DeltaTime;
	DeltaMove.Z += WorldVerticalMove * DeltaTime;
	TargetLocation += DeltaMove;

	if (Input.GetKey(VK_RBUTTON))
	{
		const float MouseRotationSpeed = 0.15f * ControlSettings.RotationSpeed;
		const float DeltaYaw = static_cast<float>(Input.MouseDeltaX()) * MouseRotationSpeed;
		const float DeltaPitch = static_cast<float>(Input.MouseDeltaY()) * MouseRotationSpeed;
		ViewTransform.Rotate(DeltaYaw, DeltaPitch);
	}

	const float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f)
	{
		if (InputSystem::Get().GetKey(VK_RBUTTON))
		{
			float& MoveSpeed = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls.MoveSpeed;
			MoveSpeed = ScrollNotches < 0.0f ? MoveSpeed * 0.9f : MoveSpeed * 1.1f;
			MoveSpeed = Clamp(MoveSpeed, 0.001f, 1000.0f);
		}
		else if (ViewTransform.bIsOrtho)
		{
			const float NewWidth = ViewTransform.OrthoZoom - ScrollNotches * ControlSettings.ZoomSpeed * DeltaTime;
			ViewTransform.OrthoZoom = Clamp(NewWidth, 0.1f, 1000.0f);
		}
		else
		{
			TargetLocation += ViewTransform.ViewRotation.GetForwardVector() * (ScrollNotches * ControlSettings.ZoomSpeed * 0.015f);
		}
	}

	if (Input.GetKeyUp(VK_SPACE) && Gizmo && bBodySetupEditingEnabled)
	{
		Gizmo->SetNextMode();
	}
}

void FStaticMeshEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!FSlateApplication::Get().DoesClientOwnMouseInput(this) || !Gizmo || !PreviewWorld)
	{
		return;
	}

	if (!bBodySetupEditingEnabled)
	{
		return;
	}

	Gizmo->ApplyScreenSpaceScaling(ViewTransform.ViewLocation, ViewTransform.bIsOrtho, ViewTransform.OrthoZoom);
	Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(RenderOptions.ViewportType, Gizmo->GetMode()));

	ImVec2 MousePos = ImGui::GetIO().MousePos;
	const float LocalMouseX = MousePos.x - ViewportScreenRect.X;
	const float LocalMouseY = MousePos.y - ViewportScreenRect.Y;

	const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : 1.0f;
	const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : 1.0f;

	FMinimalViewInfo POV;
	GetCameraView(POV);
	const FRay Ray = POV.DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	FHitResult HitResult;
	FRayUtils::RaycastComponent(Gizmo, Ray, HitResult);

	InputSystem& Input = InputSystem::Get();
	if (Input.GetKeyDown(VK_LBUTTON))
	{
		HandleDragStart(Ray);
	}
	else if (Input.GetLeftDragging())
	{
		if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
		{
			Gizmo->SetHolding(true);
		}

		if (Gizmo->IsHolding())
		{
			Gizmo->UpdateDrag(Ray);
			MarkBodySetupDebugDirty();
		}
	}
	else if (Input.GetLeftDragEnd())
	{
		const bool bWasHoldingShape = Gizmo->IsHolding() && IsBodySetupShapeGizmoActive();
		if (Gizmo->IsHolding())
		{
			Gizmo->DragEnd();
		}
		if (bWasHoldingShape && OnBodySetupShapeEdited)
		{
			OnBodySetupShapeEdited();
		}
	}
	else if (Input.GetKeyUp(VK_LBUTTON))
	{
		Gizmo->SetPressedOnHandle(false);
	}
}

void FStaticMeshEditorViewportClient::SyncCameraSmoothingTarget()
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const bool bCameraMovedExternally = bLastAppliedCameraLocationInitialized
		&& FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) > 0.0001f;

	if (!bTargetLocationInitialized || bCameraMovedExternally)
	{
		TargetLocation = CurrentLocation;
		bTargetLocationInitialized = true;
	}

	LastAppliedCameraLocation = CurrentLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FStaticMeshEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	ViewTransform.ViewLocation = NewLocation;

	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}

struct FStaticMeshEditorViewportClient::FBodySetupShapeHitResult
{
	FBodySetupShapeSelection Selection;
	float Distance = FLT_MAX;
};

void FStaticMeshEditorViewportClient::SyncBodySetupShapeGizmoTarget()
{
	if (!Gizmo)
	{
		return;
	}

	if (!bBodySetupEditingEnabled || !PreviewMeshComponent || !SelectedBodySetupShape.IsValid())
	{
		if (IsBodySetupShapeGizmoActive())
		{
			BodySetupShapeTarget.Clear();
			Gizmo->Deactivate();
		}
		return;
	}

	BodySetupShapeTarget.SetShape(PreviewMeshComponent, SelectedBodySetupShape);
	if (!BodySetupShapeTarget.IsValid())
	{
		if (IsBodySetupShapeGizmoActive())
		{
			Gizmo->Deactivate();
		}
		BodySetupShapeTarget.Clear();
		return;
	}

	if (Gizmo->GetTarget() != &BodySetupShapeTarget)
	{
		Gizmo->SetTarget(&BodySetupShapeTarget);
	}
	else
	{
		Gizmo->UpdateGizmoTransform();
	}
	ApplyTransformSettingsToGizmo();
}

void FStaticMeshEditorViewportClient::NotifyBodySetupShapePicked(FBodySetupShapeSelection InSelection)
{
	SelectedBodySetupShape = InSelection;
	SyncBodySetupShapeGizmoTarget();
	if (OnBodySetupShapePicked)
	{
		OnBodySetupShapePicked(InSelection);
	}
}

void FStaticMeshEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	FHitResult Hit;
	if (FRayUtils::RaycastComponent(Gizmo, Ray, Hit))
	{
		Gizmo->SetPressedOnHandle(true);
		return;
	}

	FBodySetupShapeHitResult ShapeHit;
	if (PickBodySetupShape(Ray, ShapeHit))
	{
		NotifyBodySetupShapePicked(ShapeHit.Selection);
	}
	else
	{
		NotifyBodySetupShapePicked({});
	}
}

bool FStaticMeshEditorViewportClient::PickBodySetupShape(const FRay& Ray, FBodySetupShapeHitResult& OutHit) const
{
	OutHit = {};
	const UBodySetup* BodySetup = GetStaticMeshBodySetup(PreviewMeshComponent);
	if (!BodySetup)
	{
		return false;
	}

	const FRay NormalizedRay = MakeNormalizedRay(Ray);
	if (NormalizedRay.Direction.IsNearlyZero())
	{
		return false;
	}

	FTransform ComponentTM = FTransform::FromMatrixWithScale(PreviewMeshComponent->GetWorldMatrix());
	const FVector Scale3D = ComponentTM.Scale;
	const float UniformScale = GetSafeUniformScale(Scale3D);
	ComponentTM.Scale = FVector::OneVector;

	const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
	for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(AggGeom.SphereElems.size()); ++ShapeIndex)
	{
		const FKSphereElem& SphereElem = AggGeom.SphereElems[ShapeIndex];
		const FVector WorldCenter = ComponentTM.TransformPosition(SphereElem.Center * UniformScale);
		float Distance = 0.0f;
		if (IntersectSphere(NormalizedRay, WorldCenter, SphereElem.Radius * UniformScale, Distance) &&
			ShouldUseHit(Distance, OutHit.Distance))
		{
			OutHit.Selection = { EAggCollisionShape::Sphere, ShapeIndex };
			OutHit.Distance = Distance;
		}
	}

	for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(AggGeom.BoxElems.size()); ++ShapeIndex)
	{
		const FKBoxElem& BoxElem = AggGeom.BoxElems[ShapeIndex];
		FTransform ShapeWorldTM = FTransform(BoxElem.Center * Scale3D, BoxElem.Rotation) * ComponentTM;
		const FVector HalfExtent(
			BoxElem.X * 0.5f * FMath::Abs(Scale3D.X),
			BoxElem.Y * 0.5f * FMath::Abs(Scale3D.Y),
			BoxElem.Z * 0.5f * FMath::Abs(Scale3D.Z));
		float Distance = 0.0f;
		if (IntersectBox(NormalizedRay, ShapeWorldTM, HalfExtent, Distance) &&
			ShouldUseHit(Distance, OutHit.Distance))
		{
			OutHit.Selection = { EAggCollisionShape::Box, ShapeIndex };
			OutHit.Distance = Distance;
		}
	}

	for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(AggGeom.SphylElems.size()); ++ShapeIndex)
	{
		const FKSphylElem& SphylElem = AggGeom.SphylElems[ShapeIndex];
		FTransform ShapeWorldTM = FTransform(SphylElem.Center * UniformScale, SphylElem.Rotation) * ComponentTM;
		float Distance = 0.0f;
		if (IntersectCapsule(
				NormalizedRay,
				ShapeWorldTM,
				SphylElem.Radius * UniformScale,
				SphylElem.Length * UniformScale,
				Distance) &&
			ShouldUseHit(Distance, OutHit.Distance))
		{
			OutHit.Selection = { EAggCollisionShape::Sphyl, ShapeIndex };
			OutHit.Distance = Distance;
		}
	}

	for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(AggGeom.ConvexElems.size()); ++ShapeIndex)
	{
		const FKConvexElem& ConvexElem = AggGeom.ConvexElems[ShapeIndex];
		FTransform ShapeWorldTM = ConvexElem.GetTransform() * ComponentTM;
		ShapeWorldTM.Scale *= UniformScale;
		float Distance = 0.0f;
		if (IntersectConvex(NormalizedRay, ConvexElem, ShapeWorldTM, Distance) &&
			ShouldUseHit(Distance, OutHit.Distance))
		{
			OutHit.Selection = { EAggCollisionShape::Convex, ShapeIndex };
			OutHit.Distance = Distance;
		}
	}

	return OutHit.Selection.IsValid();
}

bool FStaticMeshEditorViewportClient::IsBodySetupShapeGizmoActive() const
{
	return Gizmo && Gizmo->GetTarget() == &BodySetupShapeTarget;
}
