#include "Gizmo/PhysicsAssetShapeGizmoTarget.h"

#include "Component/Debug/PhysicsAssetDebugComponent.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include <algorithm>

namespace
{
	constexpr float MinShapeSize = 0.1f;

	float GetSafeUniformScale(float UniformScale)
	{
		return FMath::Abs(UniformScale) > FMath::KINDA_SMALL_NUMBER ? UniformScale : 1.0f;
	}

	FTransform GetLocalTransformFromWorld(const FTransform& WorldTM, const FTransform& BoneTM)
	{
		return FTransform::FromMatrixWithScale(WorldTM.ToMatrix() * BoneTM.ToMatrix().GetAffineInverse());
	}
}

struct FPhysicsAssetShapeGizmoTarget::FEditableShape
{
	EAggCollisionShape Type = EAggCollisionShape::Unknown;
	int32 ShapeIndex = -1;
	FKShapeElem* Shape = nullptr;
};

FPhysicsAssetShapeGizmoTarget::FPhysicsAssetShapeGizmoTarget(
	UPhysicsAssetDebugComponent* InDebugComponent,
	int32 InBodyIndex)
	: DebugComponent(InDebugComponent)
	, BodyIndex(InBodyIndex)
{
}

void FPhysicsAssetShapeGizmoTarget::SetShape(UPhysicsAssetDebugComponent* InDebugComponent, int32 InBodyIndex)
{
	DebugComponent = InDebugComponent;
	BodyIndex = InBodyIndex;
}

void FPhysicsAssetShapeGizmoTarget::Clear()
{
	DebugComponent = nullptr;
	BodyIndex = -1;
}

bool FPhysicsAssetShapeGizmoTarget::IsValid() const
{
	FEditableShape Shape;
	return GetEditableShape(Shape);
}

UWorld* FPhysicsAssetShapeGizmoTarget::GetWorld() const
{
	UPhysicsAssetDebugComponent* Component = GetDebugComponent();
	return Component ? Component->GetWorld() : nullptr;
}

FVector FPhysicsAssetShapeGizmoTarget::GetWorldLocation() const
{
	return GetShapeWorldTransform().Location;
}

FRotator FPhysicsAssetShapeGizmoTarget::GetWorldRotation() const
{
	return GetShapeWorldTransform().GetRotator();
}

FQuat FPhysicsAssetShapeGizmoTarget::GetWorldQuat() const
{
	return GetShapeWorldTransform().Rotation;
}

FVector FPhysicsAssetShapeGizmoTarget::GetWorldScale() const
{
	return GetShapeWorldTransform().Scale;
}

void FPhysicsAssetShapeGizmoTarget::SetWorldLocation(const FVector& NewLocation)
{
	FEditableShape Shape;
	FTransform BoneTM;
	float UniformScale = 1.0f;
	if (!GetEditableShape(Shape) || !GetBoneEditTransform(BoneTM, UniformScale))
	{
		return;
	}

	UniformScale = GetSafeUniformScale(UniformScale);
	const FVector BoneLocalLocation = BoneTM.InverseTransformPositionNoScale(NewLocation);

	switch (Shape.Type)
	{
	case EAggCollisionShape::Sphere:
		static_cast<FKSphereElem*>(Shape.Shape)->Center = BoneLocalLocation / UniformScale;
		break;
	case EAggCollisionShape::Box:
		static_cast<FKBoxElem*>(Shape.Shape)->Center = BoneLocalLocation / UniformScale;
		break;
	case EAggCollisionShape::Sphyl:
		static_cast<FKSphylElem*>(Shape.Shape)->Center = BoneLocalLocation / UniformScale;
		break;
	case EAggCollisionShape::Convex:
	{
		FKConvexElem* ConvexElem = static_cast<FKConvexElem*>(Shape.Shape);
		FTransform LocalTM = ConvexElem->GetTransform();
		LocalTM.Location = BoneLocalLocation;
		ConvexElem->SetTransform(LocalTM);
		break;
	}
	default:
		return;
	}

	MarkShapeChanged();
}

void FPhysicsAssetShapeGizmoTarget::SetWorldRotation(const FRotator& NewRotation)
{
	SetWorldRotation(NewRotation.ToQuaternion());
}

void FPhysicsAssetShapeGizmoTarget::SetWorldRotation(const FQuat& NewQuat)
{
	FEditableShape Shape;
	FTransform BoneTM;
	float UniformScale = 1.0f;
	if (!GetEditableShape(Shape) || !GetBoneEditTransform(BoneTM, UniformScale))
	{
		return;
	}

	if (Shape.Type == EAggCollisionShape::Sphere)
	{
		return;
	}

	UniformScale = GetSafeUniformScale(UniformScale);
	const FTransform CurrentWorldTM = GetShapeWorldTransform();
	const FTransform DesiredWorldTM(CurrentWorldTM.Location, NewQuat, CurrentWorldTM.Scale);
	FTransform LocalTM = GetLocalTransformFromWorld(DesiredWorldTM, BoneTM);

	switch (Shape.Type)
	{
	case EAggCollisionShape::Box:
	{
		FKBoxElem* BoxElem = static_cast<FKBoxElem*>(Shape.Shape);
		BoxElem->Center = LocalTM.Location / UniformScale;
		BoxElem->Rotation = LocalTM.GetRotator();
		break;
	}
	case EAggCollisionShape::Sphyl:
	{
		FKSphylElem* SphylElem = static_cast<FKSphylElem*>(Shape.Shape);
		SphylElem->Center = LocalTM.Location / UniformScale;
		SphylElem->Rotation = LocalTM.GetRotator();
		break;
	}
	case EAggCollisionShape::Convex:
	{
		FKConvexElem* ConvexElem = static_cast<FKConvexElem*>(Shape.Shape);
		LocalTM.Scale /= UniformScale;
		ConvexElem->SetTransform(LocalTM);
		break;
	}
	default:
		return;
	}

	MarkShapeChanged();
}

void FPhysicsAssetShapeGizmoTarget::SetWorldScale(const FVector& NewScale)
{
	AddScaleDelta(NewScale - GetWorldScale());
}

void FPhysicsAssetShapeGizmoTarget::AddWorldOffset(const FVector& Delta)
{
	SetWorldLocation(GetWorldLocation() + Delta);
}

void FPhysicsAssetShapeGizmoTarget::AddWorldRotation(const FQuat& Delta, bool bWorldSpace)
{
	if (!IsValid())
	{
		return;
	}

	const FQuat CurrentQuat = GetWorldQuat();
	const FQuat NewQuat = bWorldSpace ? Delta * CurrentQuat : CurrentQuat * Delta;
	SetWorldRotation(NewQuat);
}

void FPhysicsAssetShapeGizmoTarget::AddScaleDelta(const FVector& Delta)
{
	FEditableShape Shape;
	FTransform BoneTM;
	float UniformScale = 1.0f;
	if (!GetEditableShape(Shape) || !GetBoneEditTransform(BoneTM, UniformScale))
	{
		return;
	}

	const FVector LocalDelta = Delta / GetSafeUniformScale(UniformScale);

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

EAggCollisionShape FPhysicsAssetShapeGizmoTarget::GetShapeType() const
{
	FEditableShape Shape;
	return GetEditableShape(Shape) ? Shape.Type : EAggCollisionShape::Unknown;
}

UPhysicsAssetDebugComponent* FPhysicsAssetShapeGizmoTarget::GetDebugComponent() const
{
	return DebugComponent.Get();
}

UBodySetup* FPhysicsAssetShapeGizmoTarget::GetBodySetup() const
{
	UPhysicsAssetDebugComponent* Component = GetDebugComponent();
	UPhysicsAsset* PhysicsAsset = Component ? Component->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset || BodyIndex < 0)
	{
		return nullptr;
	}

	TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetupsMutable();
	if (BodyIndex >= static_cast<int32>(BodySetups.size()))
	{
		return nullptr;
	}

	return BodySetups[BodyIndex];
}

bool FPhysicsAssetShapeGizmoTarget::GetBoneEditTransform(FTransform& OutBoneTM, float& OutUniformScale) const
{
	UPhysicsAssetDebugComponent* Component = GetDebugComponent();
	UBodySetup* BodySetup = GetBodySetup();
	if (!Component || !BodySetup)
	{
		return false;
	}

	if (!Component->GetPhysicsAssetBoneWorldTransform(BodySetup->BoneName, OutBoneTM))
	{
		return false;
	}

	OutUniformScale = GetSafeUniformScale(OutBoneTM.Scale.GetAbsMax());
	OutBoneTM.Scale = FVector::OneVector;
	return true;
}

bool FPhysicsAssetShapeGizmoTarget::GetEditableShape(FEditableShape& OutShape) const
{
	UBodySetup* BodySetup = GetBodySetup();
	if (!BodySetup)
	{
		return false;
	}

	FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
	if (!AggGeom.SphereElems.empty())
	{
		OutShape = { EAggCollisionShape::Sphere, 0, &AggGeom.SphereElems[0] };
		return true;
	}
	if (!AggGeom.BoxElems.empty())
	{
		OutShape = { EAggCollisionShape::Box, 0, &AggGeom.BoxElems[0] };
		return true;
	}
	if (!AggGeom.SphylElems.empty())
	{
		OutShape = { EAggCollisionShape::Sphyl, 0, &AggGeom.SphylElems[0] };
		return true;
	}
	if (!AggGeom.ConvexElems.empty())
	{
		OutShape = { EAggCollisionShape::Convex, 0, &AggGeom.ConvexElems[0] };
		return true;
	}

	return false;
}

FTransform FPhysicsAssetShapeGizmoTarget::GetShapeWorldTransform() const
{
	FEditableShape Shape;
	FTransform BoneTM;
	float UniformScale = 1.0f;
	if (!GetEditableShape(Shape) || !GetBoneEditTransform(BoneTM, UniformScale))
	{
		return FTransform();
	}

	UniformScale = GetSafeUniformScale(UniformScale);

	switch (Shape.Type)
	{
	case EAggCollisionShape::Sphere:
	{
		const FKSphereElem* SphereElem = static_cast<const FKSphereElem*>(Shape.Shape);
		const FVector WorldCenter = BoneTM.TransformPosition(SphereElem->Center * UniformScale);
		const float Diameter = std::max(SphereElem->Radius * 2.0f * UniformScale, MinShapeSize);
		return FTransform(WorldCenter, BoneTM.Rotation, FVector(Diameter, Diameter, Diameter));
	}
	case EAggCollisionShape::Box:
	{
		const FKBoxElem* BoxElem = static_cast<const FKBoxElem*>(Shape.Shape);
		FTransform ShapeWorldTM = FTransform(BoxElem->Center * UniformScale, BoxElem->Rotation) * BoneTM;
		ShapeWorldTM.Scale = FVector(BoxElem->X * UniformScale, BoxElem->Y * UniformScale, BoxElem->Z * UniformScale);
		return ShapeWorldTM;
	}
	case EAggCollisionShape::Sphyl:
	{
		const FKSphylElem* SphylElem = static_cast<const FKSphylElem*>(Shape.Shape);
		FTransform ShapeWorldTM = FTransform(SphylElem->Center * UniformScale, SphylElem->Rotation) * BoneTM;
		const float Diameter = SphylElem->Radius * 2.0f * UniformScale;
		ShapeWorldTM.Scale = FVector(Diameter, Diameter, (SphylElem->Length + SphylElem->Radius * 2.0f) * UniformScale);
		return ShapeWorldTM;
	}
	case EAggCollisionShape::Convex:
	{
		const FKConvexElem* ConvexElem = static_cast<const FKConvexElem*>(Shape.Shape);
		FTransform ShapeWorldTM = ConvexElem->GetTransform() * BoneTM;
		ShapeWorldTM.Scale *= UniformScale;
		return ShapeWorldTM;
	}
	default:
		return FTransform();
	}
}

void FPhysicsAssetShapeGizmoTarget::MarkShapeChanged() const
{
	if (UPhysicsAssetDebugComponent* Component = GetDebugComponent())
	{
		Component->MarkRenderStateDirty();
	}
}
