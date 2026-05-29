#include "Physics/PhysicsShape.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysXConversions.h"

#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"

void FPhysicsShapeFactory::CreateShapesForComponent(physx::PxPhysics& Physics, physx::PxMaterial& Material,
	UPrimitiveComponent* Component, bool bTrigger, TArray<physx::PxShape*>& OutShapes)
{
	if (!Component) return;

	if (UBoxComponent* Box = Cast<UBoxComponent>(Component))
	{
		const FVector Extent = Box->GetScaledBoxExtent();
		physx::PxShape* Shape = Physics.createShape(physx::PxBoxGeometry(Extent.X, Extent.Y, Extent.Z), Material);
		if (Shape)
		{
			ApplyShapeFlags(*Shape, Component, bTrigger);
			OutShapes.push_back(Shape);
		}
		return;
	}
	else if (USphereComponent* Sphere = Cast<USphereComponent>(Component))
	{
		const float Radius = Sphere->GetScaledSphereRadius();
		physx::PxShape* Shape = Physics.createShape(physx::PxSphereGeometry(Radius), Material);
		if (Shape)
		{
			ApplyShapeFlags(*Shape, Component, bTrigger);
			OutShapes.push_back(Shape);
		}
		return;
	}
	else if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Component))
	{
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const float SegmentHalfLength = (HalfHeight > Radius) ? (HalfHeight - Radius) : 0.0f;

		physx::PxShape* Shape = Physics.createShape(physx::PxCapsuleGeometry(Radius, SegmentHalfLength), Material);
		if (Shape)
		{
			Shape->setLocalPose(physx::PxTransform(physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0.0f, 1.0f, 0.0f))));
			ApplyShapeFlags(*Shape, Component, bTrigger);
			OutShapes.push_back(Shape);
		}
		return;
	}
	else if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component))
	{
		CreateShapesForStaticMeshComponent(Physics, Material, StaticMeshComp,
			bTrigger, OutShapes);
		return;
	}
}

void FPhysicsShapeFactory::CreateShapesFromBodySetup(physx::PxPhysics& Physics, physx::PxMaterial& Material,
	const UBodySetup& BodySetup, const FVector& Scale, UPrimitiveComponent* UserDataComponent,
	bool bTrigger, TArray<physx::PxShape*>& OutShapes)
{
	if (!BodySetup.HasSimpleCollision()) return;

	const FKAggregateGeom& AggGeom = BodySetup.GetAggGeom();
	const FVector AbsScale(std::abs(Scale.X), std::abs(Scale.Y), std::abs(Scale.Z));

	for (const FKBoxElem& Box : AggGeom.BoxElems)
	{
		const FVector Extent(
			Box.Extents.X * AbsScale.X,
			Box.Extents.Y * AbsScale.Y,
			Box.Extents.Z * AbsScale.Z);

		physx::PxShape* Shape = Physics.createShape(physx::PxBoxGeometry(Extent.X, Extent.Y, Extent.Z), Material);
		if (!Shape) continue;

		const FVector LocalCenter(
			Box.Center.X * AbsScale.X,
			Box.Center.Y * AbsScale.Y,
			Box.Center.Z * AbsScale.Z);

		Shape->setLocalPose(ToPxTransform(LocalCenter, Box.Rotation));
		ApplyShapeFlags(*Shape, UserDataComponent, bTrigger);
		OutShapes.push_back(Shape);
	}

	for (const FKSphereElem& Sphere : AggGeom.SphereElems)
	{
		const float MaxScale = std::max(AbsScale.X, std::max(AbsScale.Y, AbsScale.Z));

		physx::PxShape* Shape = Physics.createShape(physx::PxSphereGeometry(Sphere.Radius * MaxScale), Material);
		if (!Shape) continue;

		const FVector LocalCenter(
			Sphere.Center.X * AbsScale.X,
			Sphere.Center.Y * AbsScale.Y,
			Sphere.Center.Z * AbsScale.Z);

		Shape->setLocalPose(physx::PxTransform(ToPxVec3(LocalCenter)));
		ApplyShapeFlags(*Shape, UserDataComponent, bTrigger);
		OutShapes.push_back(Shape);
	}

	for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
	{
		const float RadiusScale = std::max(AbsScale.X, AbsScale.Y);
		const float LengthScale = AbsScale.Z;

		const float Radius = Sphyl.Radius * RadiusScale;
		const float HalfLength = Sphyl.Length * LengthScale * 0.5f;

		physx::PxShape* Shape = Physics.createShape(physx::PxCapsuleGeometry(Radius, HalfLength), Material);
		if (!Shape) continue;

		const FVector LocalCenter(
			Sphyl.Center.X * AbsScale.X,
			Sphyl.Center.Y * AbsScale.Y,
			Sphyl.Center.Z * AbsScale.Z);

		const FQuat CapsuleAxisFix = FQuat::FromAxisAngle(FVector(0.0f, 1.0f, 0.0f), 90.0f);

		const FQuat LocalRot = (Sphyl.Rotation * CapsuleAxisFix).GetNormalized();

		Shape->setLocalPose(ToPxTransform(LocalCenter, LocalRot));
		ApplyShapeFlags(*Shape, UserDataComponent, bTrigger);
		OutShapes.push_back(Shape);
	}
}

void FPhysicsShapeFactory::CreateShapesForStaticMeshComponent(physx::PxPhysics& Physics, physx::PxMaterial& Material,
	UStaticMeshComponent* Component, bool bTrigger, TArray<physx::PxShape*>& OutShapes)
{
	if (!Component) return;

	UStaticMesh* StaticMesh = Component->GetStaticMesh();
	if (!StaticMesh) return;

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (!BodySetup) return;

	CreateShapesFromBodySetup(Physics, Material, *BodySetup, Component->GetWorldScale(), Component, bTrigger, OutShapes);
}

void FPhysicsShapeFactory::ApplyShapeFlags(physx::PxShape& Shape, UPrimitiveComponent* Component, bool bTrigger)
{
	Shape.userData = Component;

	if (bTrigger)
	{
		Shape.setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
		Shape.setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
	}
}
