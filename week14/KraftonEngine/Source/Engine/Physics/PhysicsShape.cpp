#include "Physics/PhysicsShape.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysXConversions.h"

#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"

physx::PxShape* FPhysicsShapeFactory::CreateShapeForComponent(physx::PxPhysics& Physics, physx::PxMaterial& Material,
	UPrimitiveComponent* Component, bool bTrigger)
{
	if (!Component) return nullptr;

	physx::PxShape* Shape = nullptr;

	if (UBoxComponent* Box = Cast<UBoxComponent>(Component))
	{
		const FVector Extent = Box->GetScaledBoxExtent();
		Shape = Physics.createShape(physx::PxBoxGeometry(Extent.X, Extent.Y, Extent.Z), Material);
	}
	else if (USphereComponent* Sphere = Cast<USphereComponent>(Component))
	{
		const float Radius = Sphere->GetScaledSphereRadius();
		Shape = Physics.createShape(physx::PxSphereGeometry(Radius), Material);
	}
	else if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Component))
	{
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

		const float SegmentHalfLength = (HalfHeight > Radius) ? (HalfHeight - Radius) : 0.0f;

		Shape = Physics.createShape(physx::PxCapsuleGeometry(Radius, SegmentHalfLength), Material);
		if (Shape)
		{
			Shape->setLocalPose(physx::PxTransform(physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0.0f, 1.0f, 0.0f))));
		}
	}
	else if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component))
	{
		UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
		if (!StaticMesh) return nullptr;

		UBodySetup* BodySetup = StaticMesh->GetBodySetup();
		if (!BodySetup || !BodySetup->HasSimpleCollision()) return nullptr;

		const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();

		if (!AggGeom.BoxElems.empty())
		{
			const FKBoxElem& Box = AggGeom.BoxElems[0];

			const FVector Scale = StaticMeshComp->GetWorldScale();
			const FVector Extent(
				Box.Extents.X * Scale.X,
				Box.Extents.Y * Scale.Y,
				Box.Extents.Z * Scale.Z);

			Shape = Physics.createShape(physx::PxBoxGeometry(Extent.X, Extent.Y, Extent.Z), Material);

			if (Shape)
			{
				const FVector LocalCenter(
					Box.Center.X * Scale.X,
					Box.Center.Y * Scale.Y,
					Box.Center.Z * Scale.Z);

				Shape->setLocalPose(ToPxTransform(LocalCenter, Box.Rotation));
			}
		}
	}

	if (!Shape) return nullptr;

	Shape->userData = Component;

	if (bTrigger)
	{
		Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
		Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
	}

	return Shape;
}
