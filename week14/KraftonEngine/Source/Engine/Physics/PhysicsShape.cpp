#include "Physics/PhysicsShape.h"

#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"

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

	if (!Shape) return nullptr;

	Shape->userData = Component;

	if (bTrigger)
	{
		Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
		Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
	}

	return Shape;
}
