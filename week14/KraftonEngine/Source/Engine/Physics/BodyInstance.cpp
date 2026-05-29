#include "Physics/BodyInstance.h"
#include "Physics/PhysXConversions.h"

#include "Component/PrimitiveComponent.h"

bool FBodyInstance::IsDynamic() const
{
	return Body && Body->is<physx::PxRigidDynamic>() != nullptr;
}

void FBodyInstance::SetGravityEnabled(bool bEnabled)
{
	if (physx::PxRigidDynamic* Dynamic = Body ? Body->is<physx::PxRigidDynamic>() : nullptr)
	{
		Dynamic->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !bEnabled);
	}
}

void FBodyInstance::SetKinematic(bool bEnable)
{
	if (physx::PxRigidDynamic* Dynamic = Body ? Body->is<physx::PxRigidDynamic>() : nullptr)
	{
		Dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, bEnable);
		Mode = bEnable ? EBodyInstanceMode::Kinematic : EBodyInstanceMode::Dynamic;
	}
}

FTransform FBodyInstance::GetBodyTransform() const
{
	if (!Body) return FTransform();

	return FromPxTransform(Body->getGlobalPose());
}

void FBodyInstance::SetBodyTransform(const FTransform& Transform)
{
	if (!Body) return;

	const physx::PxTransform Pose = ToPxTransform(Transform.Location, Transform.Rotation);

	if (physx::PxRigidDynamic* Dynamic = Body->is<physx::PxRigidDynamic>())
	{
		if (Mode == EBodyInstanceMode::Kinematic)
		{
			Dynamic->setKinematicTarget(Pose);
		}
		else
		{
			Dynamic->setGlobalPose(Pose);
		}
		return;
	}

	Body->setGlobalPose(Pose);
}

void FBodyInstance::AddForce(const FVector& Force)
{
	if (physx::PxRigidDynamic* Dynamic = Body ? Body->is<physx::PxRigidDynamic>() : nullptr)
	{
		if (Mode == EBodyInstanceMode::Dynamic)
		{
			Dynamic->addForce(ToPxVec3(Force), physx::PxForceMode::eFORCE);
		}
	}
}

void FBodyInstance::AddImpulse(const FVector& Impulse)
{
	if (physx::PxRigidDynamic* Dynamic = Body ? Body->is<physx::PxRigidDynamic>() : nullptr)
	{
		if (Mode == EBodyInstanceMode::Dynamic)
		{
			Dynamic->addForce(ToPxVec3(Impulse), physx::PxForceMode::eIMPULSE);
		}
	}
}

void FBodyInstance::SetLinearVelocity(const FVector& Velocity)
{
	if (physx::PxRigidDynamic* Dynamic = Body ? Body->is<physx::PxRigidDynamic>() : nullptr)
	{
		if (Mode == EBodyInstanceMode::Dynamic)
		{
			Dynamic->setLinearVelocity(ToPxVec3(Velocity));
		}
	}
}

FVector FBodyInstance::GetLinearVelocity() const
{
	if (physx::PxRigidDynamic* Dynamic = Body ? Body->is<physx::PxRigidDynamic>() : nullptr)
	{
		return FromPxVec3(Dynamic->getLinearVelocity());
	}

	return FVector::ZeroVector;
}

AActor* FBodyInstance::GetOwnerActor() const
{
	return OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
}

void FBodyInstance::SyncFromPhysics()
{
	if (!OwnerComponent || !Body) return;
	if (Mode != EBodyInstanceMode::Dynamic) return;

	physx::PxRigidDynamic* DynamicBody = Body->is<physx::PxRigidDynamic>();
	if (!DynamicBody) return;

	const FTransform Transform = FromPxTransform(DynamicBody->getGlobalPose());

	OwnerComponent->SetWorldLocation(Transform.Location);
	OwnerComponent->SetRelativeRotation(Transform.Rotation);
}

void FBodyInstance::SyncToPhysics()
{
	if (!OwnerComponent || !Body) return;

	FTransform Transform(OwnerComponent->GetWorldLocation(),
		OwnerComponent->GetWorldRotation().ToQuaternion(), OwnerComponent->GetWorldScale());

	SetBodyTransform(Transform);
}
