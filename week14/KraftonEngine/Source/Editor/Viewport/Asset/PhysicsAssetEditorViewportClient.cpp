#include "Editor/Viewport/Asset/PhysicsAssetEditorViewportClient.h"

#include "Collision/Ray/RayUtils.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/World.h"
#include "Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsAssetDebugDraw.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <cfloat>
#include <cmath>

namespace
{
	float LocalMin(float A, float B)
	{
		return A < B ? A : B;
	}

	float LocalMax(float A, float B)
	{
		return A > B ? A : B;
	}

	float LocalClamp(float Value, float MinValue, float MaxValue)
	{
		return LocalMax(MinValue, LocalMin(Value, MaxValue));
	}

	bool IntersectRaySphere(const FVector& Origin, const FVector& Direction, const FVector& Center, float Radius, float& OutT)
	{
		const FVector M = Origin - Center;
		const float B = M.Dot(Direction);
		const float C = M.Dot(M) - Radius * Radius;
		if (C > 0.0f && B > 0.0f)
		{
			return false;
		}

		const float Discriminant = B * B - C;
		if (Discriminant < 0.0f)
		{
			return false;
		}

		OutT = -B - std::sqrt(Discriminant);
		if (OutT < 0.0f)
		{
			OutT = 0.0f;
		}
		return true;
	}

	bool IntersectRayAABB(const FVector& Origin, const FVector& Direction, const FVector& Extents, float& OutT)
	{
		float TMin = 0.0f;
		float TMax = FLT_MAX;

		const float O[3] = { Origin.X, Origin.Y, Origin.Z };
		const float D[3] = { Direction.X, Direction.Y, Direction.Z };
		const float E[3] = { Extents.X, Extents.Y, Extents.Z };

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (std::fabs(D[Axis]) < 1e-6f)
			{
				if (O[Axis] < -E[Axis] || O[Axis] > E[Axis])
				{
					return false;
				}
				continue;
			}

			float T1 = (-E[Axis] - O[Axis]) / D[Axis];
			float T2 = ( E[Axis] - O[Axis]) / D[Axis];
			if (T1 > T2)
			{
				const float Temp = T1;
				T1 = T2;
				T2 = Temp;
			}

			if (T1 > TMin) TMin = T1;
			if (T2 < TMax) TMax = T2;
			if (TMin > TMax)
			{
				return false;
			}
		}

		OutT = TMin;
		return true;
	}

	bool IntersectRayCapsuleZ(const FVector& Origin, const FVector& Direction, float Radius, float Length, float& OutT)
	{
		bool bHit = false;
		float BestT = FLT_MAX;
		const float HalfLength = Length * 0.5f;

		const float A = Direction.X * Direction.X + Direction.Y * Direction.Y;
		const float B = 2.0f * (Origin.X * Direction.X + Origin.Y * Direction.Y);
		const float C = Origin.X * Origin.X + Origin.Y * Origin.Y - Radius * Radius;
		const float Discriminant = B * B - 4.0f * A * C;
		if (A > 1e-6f && Discriminant >= 0.0f)
		{
			const float SqrtD = std::sqrt(Discriminant);
			const float Denom = 2.0f * A;
			const float Candidates[2] = { (-B - SqrtD) / Denom, (-B + SqrtD) / Denom };
			for (float T : Candidates)
			{
				if (T < 0.0f || T >= BestT)
				{
					continue;
				}

				const float Z = Origin.Z + Direction.Z * T;
				if (Z >= -HalfLength && Z <= HalfLength)
				{
					BestT = T;
					bHit = true;
				}
			}
		}

		float CapT = 0.0f;
		if (IntersectRaySphere(Origin, Direction, FVector(0.0f, 0.0f, HalfLength), Radius, CapT) && CapT < BestT)
		{
			BestT = CapT;
			bHit = true;
		}
		if (IntersectRaySphere(Origin, Direction, FVector(0.0f, 0.0f, -HalfLength), Radius, CapT) && CapT < BestT)
		{
			BestT = CapT;
			bHit = true;
		}

		OutT = BestT;
		return bHit;
	}

	void ConsiderHit(
		const FRay& WorldRay,
		const FMatrix& BoneMatrix,
		const FVector& BoneLocalHit,
		int32 BodyIndex,
		EPhysicsAssetShapeType ShapeType,
		int32 ShapeIndex,
		FPhysicsAssetEditorHitResult& InOutBest)
	{
		const FVector WorldHit = BoneMatrix.TransformPositionWithW(BoneLocalHit);
		const float Distance = (WorldHit - WorldRay.Origin).Dot(WorldRay.Direction);
		if (Distance < 0.0f)
		{
			return;
		}

		if (!InOutBest.bHit || Distance < InOutBest.Distance)
		{
			InOutBest.bHit = true;
			InOutBest.BodyIndex = BodyIndex;
			InOutBest.ShapeType = ShapeType;
			InOutBest.ShapeIndex = ShapeIndex;
			InOutBest.Distance = Distance;
			InOutBest.WorldPosition = WorldHit;
		}
	}

	float DistanceRaySegmentSquared(const FRay& Ray, const FVector& SegmentA, const FVector& SegmentB, float& OutRayT)
	{
		const FVector U = Ray.Direction.Normalized();
		const FVector V = SegmentB - SegmentA;
		const FVector W = Ray.Origin - SegmentA;
		const float A = U.Dot(U);
		const float B = U.Dot(V);
		const float C = V.Dot(V);
		const float D = U.Dot(W);
		const float E = V.Dot(W);
		const float Denom = A * C - B * B;

		float RayT = 0.0f;
		float SegmentT = 0.0f;
		if (C <= 1.e-8f)
		{
			RayT = LocalMax(-D, 0.0f);
			SegmentT = 0.0f;
		}
		else if (std::fabs(Denom) <= 1.e-8f)
		{
			RayT = 0.0f;
			SegmentT = LocalClamp(E / C, 0.0f, 1.0f);
		}
		else
		{
			RayT = (B * E - C * D) / Denom;
			SegmentT = (A * E - B * D) / Denom;

			if (RayT < 0.0f)
			{
				RayT = 0.0f;
				SegmentT = LocalClamp(E / C, 0.0f, 1.0f);
			}
			else
			{
				SegmentT = LocalClamp(SegmentT, 0.0f, 1.0f);
				RayT = LocalMax((B * SegmentT - D) / A, 0.0f);
			}
		}

		const FVector PointOnRay = Ray.Origin + U * RayT;
		const FVector PointOnSegment = SegmentA + V * SegmentT;
		OutRayT = RayT;
		return FVector::DistSquared(PointOnRay, PointOnSegment);
	}

	float DominantScaleDelta(const FVector& Delta)
	{
		float Result = 0.0f;
		float AbsResult = 0.0f;
		const float Values[3] = { Delta.X, Delta.Y, Delta.Z };
		for (float Value : Values)
		{
			const float AbsValue = std::fabs(Value);
			if (AbsValue > AbsResult)
			{
				AbsResult = AbsValue;
				Result = Value;
			}
		}
		return Result;
	}
}

void FPhysicsAssetEditorGizmoTarget::Clear()
{
	TargetType = ETargetType::None;
	MeshComponent = nullptr;
	Body = nullptr;
	Constraint = nullptr;
	ShapeType = EPhysicsAssetShapeType::Sphere;
	ShapeIndex = -1;
	bDirty = false;
}

void FPhysicsAssetEditorGizmoTarget::SetShape(USkeletalMeshComponent* InMeshComponent, UBodySetup* InBody, EPhysicsAssetShapeType InShapeType, int32 InShapeIndex)
{
	TargetType = ETargetType::Shape;
	MeshComponent = InMeshComponent;
	Body = InBody;
	Constraint = nullptr;
	ShapeType = InShapeType;
	ShapeIndex = InShapeIndex;
	bDirty = false;
}

void FPhysicsAssetEditorGizmoTarget::SetConstraint(USkeletalMeshComponent* InMeshComponent, UPhysicsConstraintTemplate* InConstraint)
{
	TargetType = ETargetType::Constraint;
	MeshComponent = InMeshComponent;
	Body = nullptr;
	Constraint = InConstraint;
	ShapeType = EPhysicsAssetShapeType::Sphere;
	ShapeIndex = -1;
	bDirty = false;
}

bool FPhysicsAssetEditorGizmoTarget::ConsumeDirty()
{
	const bool bWasDirty = bDirty;
	bDirty = false;
	return bWasDirty;
}

void FPhysicsAssetEditorGizmoTarget::MarkDirty()
{
	bDirty = true;
}

bool FPhysicsAssetEditorGizmoTarget::IsValid() const
{
	if (!MeshComponent)
	{
		return false;
	}

	if (TargetType == ETargetType::Constraint)
	{
		return Constraint != nullptr;
	}

	return TargetType == ETargetType::Shape
		&& Body != nullptr
		&& ShapeIndex >= 0
		&& ShapeIndex < Body->GetShapeCount(ShapeType);
}

UWorld* FPhysicsAssetEditorGizmoTarget::GetWorld() const
{
	return MeshComponent ? MeshComponent->GetWorld() : nullptr;
}

bool FPhysicsAssetEditorGizmoTarget::GetBodyBoneMatrix(FMatrix& OutMatrix) const
{
	return MeshComponent && Body
		&& MeshComponent->GetBoneWorldMatrixByName(Body->GetBoneName().ToString(), OutMatrix);
}

bool FPhysicsAssetEditorGizmoTarget::GetConstraintParentMatrix(FMatrix& OutMatrix) const
{
	return MeshComponent && Constraint
		&& MeshComponent->GetBoneWorldMatrixByName(Constraint->GetParentBoneName().ToString(), OutMatrix);
}

bool FPhysicsAssetEditorGizmoTarget::GetShapeLocalTransform(FVector& OutLocation, FQuat& OutRotation) const
{
	if (!Body || ShapeIndex < 0)
	{
		return false;
	}

	const FKAggregateGeom& Geom = Body->GetAggGeom();
	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		if (ShapeIndex >= static_cast<int32>(Geom.SphereElems.size())) return false;
		OutLocation = Geom.SphereElems[ShapeIndex].Center;
		OutRotation = FQuat::Identity;
		return true;
	case EPhysicsAssetShapeType::Box:
		if (ShapeIndex >= static_cast<int32>(Geom.BoxElems.size())) return false;
		OutLocation = Geom.BoxElems[ShapeIndex].Center;
		OutRotation = Geom.BoxElems[ShapeIndex].Rotation;
		return true;
	case EPhysicsAssetShapeType::Sphyl:
		if (ShapeIndex >= static_cast<int32>(Geom.SphylElems.size())) return false;
		OutLocation = Geom.SphylElems[ShapeIndex].Center;
		OutRotation = Geom.SphylElems[ShapeIndex].Rotation;
		return true;
	default:
		return false;
	}
}

bool FPhysicsAssetEditorGizmoTarget::SetShapeLocalLocation(const FVector& NewLocation)
{
	if (!Body || ShapeIndex < 0)
	{
		return false;
	}

	FKAggregateGeom& Geom = Body->GetAggGeom();
	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		if (ShapeIndex >= static_cast<int32>(Geom.SphereElems.size())) return false;
		Geom.SphereElems[ShapeIndex].Center = NewLocation;
		MarkDirty();
		return true;
	case EPhysicsAssetShapeType::Box:
		if (ShapeIndex >= static_cast<int32>(Geom.BoxElems.size())) return false;
		Geom.BoxElems[ShapeIndex].Center = NewLocation;
		MarkDirty();
		return true;
	case EPhysicsAssetShapeType::Sphyl:
		if (ShapeIndex >= static_cast<int32>(Geom.SphylElems.size())) return false;
		Geom.SphylElems[ShapeIndex].Center = NewLocation;
		MarkDirty();
		return true;
	default:
		return false;
	}
}

bool FPhysicsAssetEditorGizmoTarget::SetShapeLocalRotation(const FQuat& NewRotation)
{
	if (!Body || ShapeIndex < 0)
	{
		return false;
	}

	FKAggregateGeom& Geom = Body->GetAggGeom();
	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Box:
		if (ShapeIndex >= static_cast<int32>(Geom.BoxElems.size())) return false;
		Geom.BoxElems[ShapeIndex].Rotation = NewRotation.GetNormalized();
		MarkDirty();
		return true;
	case EPhysicsAssetShapeType::Sphyl:
		if (ShapeIndex >= static_cast<int32>(Geom.SphylElems.size())) return false;
		Geom.SphylElems[ShapeIndex].Rotation = NewRotation.GetNormalized();
		MarkDirty();
		return true;
	default:
		return false;
	}
}

FVector FPhysicsAssetEditorGizmoTarget::GetShapeLocalScale() const
{
	if (!Body || ShapeIndex < 0)
	{
		return FVector::OneVector;
	}

	const FKAggregateGeom& Geom = Body->GetAggGeom();
	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		if (ShapeIndex < static_cast<int32>(Geom.SphereElems.size()))
		{
			const float Radius = Geom.SphereElems[ShapeIndex].Radius;
			return FVector(Radius, Radius, Radius);
		}
		break;
	case EPhysicsAssetShapeType::Box:
		if (ShapeIndex < static_cast<int32>(Geom.BoxElems.size()))
		{
			return Geom.BoxElems[ShapeIndex].Extents;
		}
		break;
	case EPhysicsAssetShapeType::Sphyl:
		if (ShapeIndex < static_cast<int32>(Geom.SphylElems.size()))
		{
			const FKSphylElem& Capsule = Geom.SphylElems[ShapeIndex];
			return FVector(Capsule.Radius, Capsule.Radius, Capsule.Length);
		}
		break;
	default:
		break;
	}
	return FVector::OneVector;
}

bool FPhysicsAssetEditorGizmoTarget::SetShapeLocalScale(const FVector& NewScale)
{
	if (!Body || ShapeIndex < 0)
	{
		return false;
	}

	FKAggregateGeom& Geom = Body->GetAggGeom();
	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		if (ShapeIndex >= static_cast<int32>(Geom.SphereElems.size())) return false;
		Geom.SphereElems[ShapeIndex].Radius = Clamp(DominantScaleDelta(NewScale), 0.001f, 1000.0f);
		MarkDirty();
		return true;
	case EPhysicsAssetShapeType::Box:
		if (ShapeIndex >= static_cast<int32>(Geom.BoxElems.size())) return false;
		Geom.BoxElems[ShapeIndex].Extents = FVector(
			Clamp(NewScale.X, 0.001f, 1000.0f),
			Clamp(NewScale.Y, 0.001f, 1000.0f),
			Clamp(NewScale.Z, 0.001f, 1000.0f));
		MarkDirty();
		return true;
	case EPhysicsAssetShapeType::Sphyl:
		if (ShapeIndex >= static_cast<int32>(Geom.SphylElems.size())) return false;
		Geom.SphylElems[ShapeIndex].Radius = Clamp(LocalMax(NewScale.X, NewScale.Y), 0.001f, 1000.0f);
		Geom.SphylElems[ShapeIndex].Length = Clamp(NewScale.Z, 0.001f, 1000.0f);
		MarkDirty();
		return true;
	default:
		return false;
	}
}

FVector FPhysicsAssetEditorGizmoTarget::GetWorldLocation() const
{
	if (TargetType == ETargetType::Constraint)
	{
		FMatrix ParentMatrix;
		return GetConstraintParentMatrix(ParentMatrix)
			? ParentMatrix.TransformPositionWithW(Constraint->GetLocalFrameA().Location)
			: FVector::ZeroVector;
	}

	FMatrix BoneMatrix;
	FVector LocalLocation;
	FQuat LocalRotation;
	return GetBodyBoneMatrix(BoneMatrix) && GetShapeLocalTransform(LocalLocation, LocalRotation)
		? BoneMatrix.TransformPositionWithW(LocalLocation)
		: FVector::ZeroVector;
}

FRotator FPhysicsAssetEditorGizmoTarget::GetWorldRotation() const
{
	return FRotator::FromQuaternion(GetWorldQuat());
}

FQuat FPhysicsAssetEditorGizmoTarget::GetWorldQuat() const
{
	if (TargetType == ETargetType::Constraint)
	{
		FMatrix ParentMatrix;
		return GetConstraintParentMatrix(ParentMatrix)
			? (ParentMatrix.ToQuat() * Constraint->GetLocalFrameA().Rotation).GetNormalized()
			: FQuat::Identity;
	}

	FMatrix BoneMatrix;
	FVector LocalLocation;
	FQuat LocalRotation;
	return GetBodyBoneMatrix(BoneMatrix) && GetShapeLocalTransform(LocalLocation, LocalRotation)
		? (BoneMatrix.ToQuat() * LocalRotation).GetNormalized()
		: FQuat::Identity;
}

FVector FPhysicsAssetEditorGizmoTarget::GetWorldScale() const
{
	return TargetType == ETargetType::Shape ? GetShapeLocalScale() : FVector::OneVector;
}

void FPhysicsAssetEditorGizmoTarget::SetWorldLocation(const FVector& NewLocation)
{
	if (TargetType == ETargetType::Constraint)
	{
		FMatrix ParentMatrix;
		if (!GetConstraintParentMatrix(ParentMatrix))
		{
			return;
		}

		FTransform FrameA = Constraint->GetLocalFrameA();
		FrameA.Location = ParentMatrix.GetInverse().TransformPositionWithW(NewLocation);
		Constraint->SetLocalFrameA(FrameA);
		MarkDirty();
		return;
	}

	FMatrix BoneMatrix;
	if (GetBodyBoneMatrix(BoneMatrix))
	{
		SetShapeLocalLocation(BoneMatrix.GetInverse().TransformPositionWithW(NewLocation));
	}
}

void FPhysicsAssetEditorGizmoTarget::SetWorldRotation(const FRotator& NewRotation)
{
	SetWorldRotation(NewRotation.ToQuaternion());
}

void FPhysicsAssetEditorGizmoTarget::SetWorldRotation(const FQuat& NewQuat)
{
	if (TargetType == ETargetType::Constraint)
	{
		FMatrix ParentMatrix;
		if (!GetConstraintParentMatrix(ParentMatrix))
		{
			return;
		}

		FTransform FrameA = Constraint->GetLocalFrameA();
		FrameA.Rotation = (ParentMatrix.ToQuat().Inverse() * NewQuat).GetNormalized();
		Constraint->SetLocalFrameA(FrameA);
		MarkDirty();
		return;
	}

	FMatrix BoneMatrix;
	if (GetBodyBoneMatrix(BoneMatrix))
	{
		SetShapeLocalRotation((BoneMatrix.ToQuat().Inverse() * NewQuat).GetNormalized());
	}
}

void FPhysicsAssetEditorGizmoTarget::SetWorldScale(const FVector& NewScale)
{
	if (TargetType == ETargetType::Shape)
	{
		SetShapeLocalScale(NewScale);
	}
}

void FPhysicsAssetEditorGizmoTarget::AddWorldOffset(const FVector& Delta)
{
	SetWorldLocation(GetWorldLocation() + Delta);
}

void FPhysicsAssetEditorGizmoTarget::AddWorldRotation(const FQuat& Delta, bool bWorldSpace)
{
	const FQuat Current = GetWorldQuat();
	SetWorldRotation(bWorldSpace ? (Delta * Current) : (Current * Delta));
}

void FPhysicsAssetEditorGizmoTarget::AddScaleDelta(const FVector& Delta)
{
	if (TargetType != ETargetType::Shape)
	{
		return;
	}

	FVector Scale = GetShapeLocalScale();
	if (ShapeType == EPhysicsAssetShapeType::Sphere)
	{
		const float Radius = Scale.X + DominantScaleDelta(Delta);
		SetShapeLocalScale(FVector(Radius, Radius, Radius));
		return;
	}

	if (ShapeType == EPhysicsAssetShapeType::Sphyl)
	{
		const float RadiusDelta = (std::fabs(Delta.X) > std::fabs(Delta.Y)) ? Delta.X : Delta.Y;
		Scale.X += RadiusDelta;
		Scale.Y += RadiusDelta;
		Scale.Z += Delta.Z;
		SetShapeLocalScale(Scale);
		return;
	}

	Scale += Delta;
	SetShapeLocalScale(Scale);
}

void FPhysicsAssetEditorViewportClient::Initialize(ID3D11Device* Device, uint32 Width, uint32 Height)
{
	Viewport = new FViewport();
	Viewport->Initialize(Device, Width, Height);
	Viewport->SetClient(this);

	RenderOptions.ShowFlags.bDebugDraw = true;
	RenderOptions.ShowFlags.bShowCollisionShape = true;
	bIsRenderable = true;
}

void FPhysicsAssetEditorViewportClient::Release()
{
	ClearGizmoSelection();
	if (Gizmo)
	{
		Gizmo->DestroyRenderState();
		UObjectManager::Get().DestroyObject(Gizmo);
		Gizmo = nullptr;
	}

	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	PreviewWorld = nullptr;
	PhysicsAsset = nullptr;
	PreviewMeshComponent = nullptr;
	bIsRenderable = false;
}

void FPhysicsAssetEditorViewportClient::SetPreviewScene(UWorld* InWorld, UPhysicsAsset* InAsset, USkeletalMeshComponent* InMeshComponent)
{
	PreviewWorld = InWorld;
	PhysicsAsset = InAsset;
	PreviewMeshComponent = InMeshComponent;
	SetShowPreviewMesh(bShowPreviewMesh);
}

void FPhysicsAssetEditorViewportClient::CreatePreviewGizmo()
{
	if (!PreviewWorld || Gizmo)
	{
		return;
	}

	Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	Gizmo->SetScene(&PreviewWorld->GetScene());
	Gizmo->CreateRenderState();
	Gizmo->Deactivate();
	ApplyTransformSettingsToGizmo();
}

void FPhysicsAssetEditorViewportClient::ResetCameraToPreviewBounds()
{
	if (!PreviewMeshComponent)
	{
		ViewTransform.ViewLocation = FVector(-5.0f, -5.0f, 3.0f);
		ViewTransform.LookAt(FVector::ZeroVector);
		TargetLocation = ViewTransform.ViewLocation;
		LastAppliedCameraLocation = ViewTransform.ViewLocation;
		bTargetLocationInitialized = true;
		bLastAppliedCameraLocationInitialized = true;
		return;
	}

	const FBoundingBox Bounds = PreviewMeshComponent->GetWorldBoundingBox();
	const FVector Center = Bounds.GetCenter();
	float Radius = Bounds.GetExtent().Length();
	if (Radius < 0.1f)
	{
		Radius = 1.0f;
	}

	const float Distance = Radius / std::tan(ViewTransform.FOV * 0.5f) * 1.35f;
	const FVector ViewDir = FVector(-1.0f, -1.0f, -0.55f).Normalized();

	ViewTransform.ViewLocation = Center - ViewDir * Distance;
	ViewTransform.LookAt(Center);

	TargetLocation = ViewTransform.ViewLocation;
	LastAppliedCameraLocation = ViewTransform.ViewLocation;
	bTargetLocationInitialized = true;
	bLastAppliedCameraLocationInitialized = true;
}

void FPhysicsAssetEditorViewportClient::SetShowPreviewMesh(bool bInShow)
{
	bShowPreviewMesh = bInShow;
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetVisibility(bShowPreviewMesh);
	}
}

bool FPhysicsAssetEditorViewportClient::GetMouseRay(FRay& OutRay) const
{
	if (!Viewport || ViewportScreenRect.Width <= 1.0f || ViewportScreenRect.Height <= 1.0f)
	{
		return false;
	}

	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	const float LocalMouseX = MousePos.x - ViewportScreenRect.X;
	const float LocalMouseY = MousePos.y - ViewportScreenRect.Y;
	if (LocalMouseX < 0.0f || LocalMouseY < 0.0f || LocalMouseX >= ViewportScreenRect.Width || LocalMouseY >= ViewportScreenRect.Height)
	{
		return false;
	}

	FMinimalViewInfo POV;
	GetCameraView(POV);
	OutRay = POV.DeprojectScreenToWorld(
		LocalMouseX,
		LocalMouseY,
		static_cast<float>(Viewport->GetWidth()),
		static_cast<float>(Viewport->GetHeight()));
	return true;
}

bool FPhysicsAssetEditorViewportClient::IsGizmoHandleAtMouse() const
{
	if (!Gizmo || !Gizmo->HasTarget())
	{
		return false;
	}

	FRay Ray;
	if (!GetMouseRay(Ray))
	{
		return false;
	}

	FHitResult Hit;
	return FRayUtils::RaycastComponent(Gizmo, Ray, Hit);
}

bool FPhysicsAssetEditorViewportClient::PickBodyShapeAtMouse(FPhysicsAssetEditorHitResult& OutHit) const
{
	OutHit = FPhysicsAssetEditorHitResult {};
	if (!PhysicsAsset || !PreviewMeshComponent)
	{
		return false;
	}

	FRay WorldRay;
	if (!GetMouseRay(WorldRay))
	{
		return false;
	}

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
	{
		const UBodySetup* Body = Bodies[BodyIndex];
		if (!Body)
		{
			continue;
		}

		FMatrix BoneMatrix;
		if (!PreviewMeshComponent->GetBoneWorldMatrixByName(Body->GetBoneName().ToString(), BoneMatrix))
		{
			continue;
		}

		const FMatrix InvBone = BoneMatrix.GetInverse();
		const FVector LocalOrigin = InvBone.TransformPositionWithW(WorldRay.Origin);
		const FVector LocalEnd = InvBone.TransformPositionWithW(WorldRay.Origin + WorldRay.Direction);
		const FVector LocalDir = (LocalEnd - LocalOrigin).Normalized();
		if (LocalDir.IsNearlyZero())
		{
			continue;
		}

		const FKAggregateGeom& Geom = Body->GetAggGeom();

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.SphereElems.size()); ++ShapeIndex)
		{
			const FKSphereElem& Sphere = Geom.SphereElems[ShapeIndex];
			float T = 0.0f;
			if (IntersectRaySphere(LocalOrigin, LocalDir, Sphere.Center, Sphere.Radius, T))
			{
				ConsiderHit(WorldRay, BoneMatrix, LocalOrigin + LocalDir * T, BodyIndex, EPhysicsAssetShapeType::Sphere, ShapeIndex, OutHit);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.BoxElems.size()); ++ShapeIndex)
		{
			const FKBoxElem& Box = Geom.BoxElems[ShapeIndex];
			const FQuat InvRot = Box.Rotation.Inverse();
			const FVector ShapeOrigin = InvRot.RotateVector(LocalOrigin - Box.Center);
			const FVector ShapeDir = InvRot.RotateVector(LocalDir).Normalized();
			float T = 0.0f;
			if (IntersectRayAABB(ShapeOrigin, ShapeDir, Box.Extents, T))
			{
				const FVector ShapeHit = ShapeOrigin + ShapeDir * T;
				const FVector BoneLocalHit = Box.Center + Box.Rotation.RotateVector(ShapeHit);
				ConsiderHit(WorldRay, BoneMatrix, BoneLocalHit, BodyIndex, EPhysicsAssetShapeType::Box, ShapeIndex, OutHit);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.SphylElems.size()); ++ShapeIndex)
		{
			const FKSphylElem& Capsule = Geom.SphylElems[ShapeIndex];
			const FQuat InvRot = Capsule.Rotation.Inverse();
			const FVector ShapeOrigin = InvRot.RotateVector(LocalOrigin - Capsule.Center);
			const FVector ShapeDir = InvRot.RotateVector(LocalDir).Normalized();
			float T = 0.0f;
			if (IntersectRayCapsuleZ(ShapeOrigin, ShapeDir, Capsule.Radius, Capsule.Length, T))
			{
				const FVector ShapeHit = ShapeOrigin + ShapeDir * T;
				const FVector BoneLocalHit = Capsule.Center + Capsule.Rotation.RotateVector(ShapeHit);
				ConsiderHit(WorldRay, BoneMatrix, BoneLocalHit, BodyIndex, EPhysicsAssetShapeType::Sphyl, ShapeIndex, OutHit);
			}
		}
	}

	return OutHit.bHit;
}

bool FPhysicsAssetEditorViewportClient::PickConstraintAtMouse(int32& OutConstraintIndex) const
{
	OutConstraintIndex = -1;
	if (!PhysicsAsset || !PreviewMeshComponent)
	{
		return false;
	}

	FRay WorldRay;
	if (!GetMouseRay(WorldRay))
	{
		return false;
	}

	float BestRayT = FLT_MAX;
	const TArray<UPhysicsConstraintTemplate*>& Constraints = PhysicsAsset->GetConstraintTemplates();
	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
	{
		const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
		if (!Constraint)
		{
			continue;
		}

		FMatrix ParentMat;
		FMatrix ChildMat;
		if (!PreviewMeshComponent->GetBoneWorldMatrixByName(Constraint->GetParentBoneName().ToString(), ParentMat)
			|| !PreviewMeshComponent->GetBoneWorldMatrixByName(Constraint->GetChildBoneName().ToString(), ChildMat))
		{
			continue;
		}

		const FVector ParentOrigin = ParentMat.GetLocation();
		const FVector ChildOrigin = ChildMat.GetLocation();
		const FVector JointWorld = ParentMat.TransformPositionWithW(Constraint->GetLocalFrameA().Location);
		const float SegmentLength = LocalMax((JointWorld - ParentOrigin).Length(), (ChildOrigin - JointWorld).Length());
		const float PickRadius = LocalMax(SegmentLength * 0.045f, 0.025f);
		const float PickRadiusSq = PickRadius * PickRadius;

		float RayT0 = 0.0f;
		const float DistSq0 = DistanceRaySegmentSquared(WorldRay, ParentOrigin, JointWorld, RayT0);
		float RayT1 = 0.0f;
		const float DistSq1 = DistanceRaySegmentSquared(WorldRay, JointWorld, ChildOrigin, RayT1);
		const float RayT = DistSq0 <= DistSq1 ? RayT0 : RayT1;
		const float DistSq = LocalMin(DistSq0, DistSq1);

		if (DistSq <= PickRadiusSq && RayT < BestRayT)
		{
			BestRayT = RayT;
			OutConstraintIndex = ConstraintIndex;
		}
	}

	return OutConstraintIndex >= 0;
}

void FPhysicsAssetEditorViewportClient::ClearHighlight()
{
	HighlightBodyIndex = -1;
	HighlightShapeIndex = -1;
	HighlightConstraintIndex = -1;
}

void FPhysicsAssetEditorViewportClient::SetHighlightedBody(int32 BodyIndex)
{
	ClearHighlight();
	HighlightBodyIndex = BodyIndex;
}

void FPhysicsAssetEditorViewportClient::SetHighlightedShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex)
{
	ClearHighlight();
	HighlightBodyIndex = BodyIndex;
	HighlightShapeType = ShapeType;
	HighlightShapeIndex = ShapeIndex;
}

void FPhysicsAssetEditorViewportClient::SetHighlightedConstraint(int32 ConstraintIndex)
{
	ClearHighlight();
	HighlightConstraintIndex = ConstraintIndex;
}

bool FPhysicsAssetEditorViewportClient::IsMouseOverViewport() const
{
	if (!bIsRenderable || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
	{
		return false;
	}

	const ImVec2 MousePos = ImGui::GetMousePos();
	return MousePos.x >= ViewportScreenRect.X && MousePos.x <= (ViewportScreenRect.X + ViewportScreenRect.Width)
		&& MousePos.y >= ViewportScreenRect.Y && MousePos.y <= (ViewportScreenRect.Y + ViewportScreenRect.Height);
}

void FPhysicsAssetEditorViewportClient::NotifyViewportResized(int32 NewWidth, int32 NewHeight)
{
	if (NewHeight > 0)
	{
		ViewTransform.AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
	}
}

bool FPhysicsAssetEditorViewportClient::GetCameraView(FMinimalViewInfo& OutPOV) const
{
	OutPOV.Location = ViewTransform.ViewLocation;
	OutPOV.Rotation = ViewTransform.ViewRotation;
	OutPOV.FOV = ViewTransform.FOV;
	OutPOV.AspectRatio = ViewTransform.AspectRatio;
	return true;
}

void FPhysicsAssetEditorViewportClient::SubmitFrameDebugDraw()
{
	DrawPreviewSkeleton();
	DrawPreviewPhysicsAsset();
}

void FPhysicsAssetEditorViewportClient::Tick(float DeltaTime)
{
	SyncCameraSmoothingTarget();
	ApplySmoothedCameraLocation(DeltaTime);
	TickShortcuts();
	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
}

void FPhysicsAssetEditorViewportClient::TickShortcuts()
{
	if (!FSlateApplication::Get().DoesClientOwnKeyboardInput(this))
	{
		return;
	}

	if (InputSystem::Get().GetKeyDown('F'))
	{
		ResetCameraToPreviewBounds();
	}

	if (Gizmo && Gizmo->HasTarget() && InputSystem::Get().GetKeyUp(VK_SPACE))
	{
		Gizmo->SetNextMode();
		ApplyTransformSettingsToGizmo();
	}
}

void FPhysicsAssetEditorViewportClient::TickInput(float DeltaTime)
{
	if (!FSlateApplication::Get().DoesClientOwnMouseInput(this))
	{
		return;
	}
	if (ImGui::GetIO().WantTextInput)
	{
		return;
	}

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
}

void FPhysicsAssetEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;
	if (!FSlateApplication::Get().DoesClientOwnMouseInput(this))
	{
		return;
	}
	if (!Gizmo || !PreviewWorld || IsSimulatingPhysics())
	{
		return;
	}
	if (!Gizmo->HasTarget())
	{
		return;
	}

	Gizmo->UpdateGizmoTransform();
	Gizmo->ApplyScreenSpaceScaling(ViewTransform.ViewLocation, ViewTransform.bIsOrtho, ViewTransform.OrthoZoom);
	Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(RenderOptions.ViewportType, Gizmo->GetMode()));

	FRay Ray;
	if (!GetMouseRay(Ray))
	{
		return;
	}

	FHitResult Hit;
	FRayUtils::RaycastComponent(Gizmo, Ray, Hit);

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
		}
	}
	else if (Input.GetLeftDragEnd())
	{
		if (Gizmo->IsHolding())
		{
			Gizmo->DragEnd();
		}
	}
	else if (Input.GetKeyUp(VK_LBUTTON))
	{
		Gizmo->SetPressedOnHandle(false);
	}
}

void FPhysicsAssetEditorViewportClient::SyncCameraSmoothingTarget()
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

void FPhysicsAssetEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	ViewTransform.ViewLocation = NewLocation;

	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FPhysicsAssetEditorViewportClient::DrawPreviewSkeleton()
{
	if (!IsShowSkeleton() || !PreviewWorld || !PreviewMeshComponent)
	{
		return;
	}

	const USkeletalMesh* Mesh = PreviewMeshComponent->GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset)
	{
		return;
	}

	const FColor BoneLineColor(255, 198, 64, 255);
	const FColor JointColor(64, 180, 255, 255);

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++BoneIndex)
	{
		const int32 ParentIndex = MeshAsset->Bones[BoneIndex].ParentIndex;
		if (ParentIndex < 0 || ParentIndex >= static_cast<int32>(MeshAsset->Bones.size()))
		{
			continue;
		}

		FMatrix ParentMatrix;
		FMatrix BoneMatrix;
		if (!PreviewMeshComponent->GetBoneWorldMatrixByIndex(ParentIndex, ParentMatrix)
			|| !PreviewMeshComponent->GetBoneWorldMatrixByIndex(BoneIndex, BoneMatrix))
		{
			continue;
		}

		const FVector ParentPos = ParentMatrix.GetLocation();
		const FVector BonePos = BoneMatrix.GetLocation();
		if (FVector::DistSquared(ParentPos, BonePos) < 1.e-6f)
		{
			continue;
		}

		DrawDebugLine(PreviewWorld, ParentPos, BonePos, BoneLineColor, 0.0f);
		DrawDebugPoint(PreviewWorld, BonePos, 0.03f, JointColor, 0.0f);
	}
}

void FPhysicsAssetEditorViewportClient::SetSimulatePhysics(bool bInSimulate)
{
	RenderOptions.ShowFlags.bPhysicsAssetSimulation = bInSimulate;
	if (bInSimulate)
	{
		ClearGizmoSelection();
	}
}

bool FPhysicsAssetEditorViewportClient::IsGizmoHolding() const
{
	return Gizmo && Gizmo->IsHolding();
}

bool FPhysicsAssetEditorViewportClient::ConsumeGizmoEdited()
{
	return GizmoTarget.ConsumeDirty();
}

void FPhysicsAssetEditorViewportClient::ApplyTransformSettingsToGizmo()
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

void FPhysicsAssetEditorViewportClient::SetGizmoBodySelection(int32 BodyIndex)
{
	if (!PhysicsAsset || !PreviewMeshComponent || !Gizmo || BodyIndex < 0)
	{
		ClearGizmoSelection();
		return;
	}

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	if (BodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[BodyIndex])
	{
		ClearGizmoSelection();
		return;
	}

	UBodySetup* Body = Bodies[BodyIndex];
	if (Body->GetShapeCount(EPhysicsAssetShapeType::Sphyl) > 0)
	{
		SetGizmoShapeSelection(BodyIndex, EPhysicsAssetShapeType::Sphyl, 0);
	}
	else if (Body->GetShapeCount(EPhysicsAssetShapeType::Box) > 0)
	{
		SetGizmoShapeSelection(BodyIndex, EPhysicsAssetShapeType::Box, 0);
	}
	else if (Body->GetShapeCount(EPhysicsAssetShapeType::Sphere) > 0)
	{
		SetGizmoShapeSelection(BodyIndex, EPhysicsAssetShapeType::Sphere, 0);
	}
	else
	{
		ClearGizmoSelection();
	}
}

void FPhysicsAssetEditorViewportClient::SetGizmoShapeSelection(int32 BodyIndex, EPhysicsAssetShapeType InShapeType, int32 InShapeIndex)
{
	if (!PhysicsAsset || !PreviewMeshComponent || !Gizmo)
	{
		ClearGizmoSelection();
		return;
	}

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[BodyIndex])
	{
		ClearGizmoSelection();
		return;
	}

	if (InShapeIndex < 0 || InShapeIndex >= Bodies[BodyIndex]->GetShapeCount(InShapeType))
	{
		ClearGizmoSelection();
		return;
	}

	GizmoTarget.SetShape(PreviewMeshComponent, Bodies[BodyIndex], InShapeType, InShapeIndex);
	Gizmo->SetTarget(&GizmoTarget);
	ApplyTransformSettingsToGizmo();
}

void FPhysicsAssetEditorViewportClient::SetGizmoConstraintSelection(int32 ConstraintIndex)
{
	if (!PhysicsAsset || !PreviewMeshComponent || !Gizmo)
	{
		ClearGizmoSelection();
		return;
	}

	const TArray<UPhysicsConstraintTemplate*>& Constraints = PhysicsAsset->GetConstraintTemplates();
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(Constraints.size()) || !Constraints[ConstraintIndex])
	{
		ClearGizmoSelection();
		return;
	}

	GizmoTarget.SetConstraint(PreviewMeshComponent, Constraints[ConstraintIndex]);
	Gizmo->SetTarget(&GizmoTarget);
	ApplyTransformSettingsToGizmo();
}

void FPhysicsAssetEditorViewportClient::ClearGizmoSelection()
{
	GizmoTarget.Clear();
	if (Gizmo)
	{
		Gizmo->Deactivate();
	}
}

void FPhysicsAssetEditorViewportClient::DrawPreviewPhysicsAsset()
{
	if (!PreviewWorld || !PhysicsAsset || !PreviewMeshComponent || (!IsShowBodies() && !IsShowConstraints()))
	{
		return;
	}

	FPhysicsAssetDebugDrawOptions Options;
	Options.bDrawBodies = IsShowBodies();
	Options.bDrawConstraints = IsShowConstraints();
	Options.HighlightBodyIndex = HighlightBodyIndex;
	Options.HighlightShapeType = HighlightShapeType;
	Options.HighlightShapeIndex = HighlightShapeIndex;
	Options.HighlightConstraintIndex = HighlightConstraintIndex;
	DrawPhysicsAssetDebug(PreviewWorld, PhysicsAsset, PreviewMeshComponent, Options);
}

void FPhysicsAssetEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	if (!Gizmo || !Gizmo->HasTarget())
	{
		return;
	}

	FHitResult Hit;
	if (FRayUtils::RaycastComponent(Gizmo, Ray, Hit))
	{
		Gizmo->SetPressedOnHandle(true);
	}
}
