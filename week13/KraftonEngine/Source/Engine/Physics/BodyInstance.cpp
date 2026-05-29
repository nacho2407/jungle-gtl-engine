#include "BodyInstance.h"

#include "PhysXTypeConversions.h"

physx::PxRigidDynamic* FBodyInstance::GetRigidDynamic() const
{
	return RigidActor ? RigidActor->is<physx::PxRigidDynamic>() : nullptr;
}

physx::PxRigidStatic* FBodyInstance::GetRigidStatic() const
{
	return RigidActor ? RigidActor->is<physx::PxRigidStatic>() : nullptr;
}

FTransform FBodyInstance::GetBodyTransform() const
{
	if (!RigidActor)
	{
		return FTransform();
	}

	const physx::PxTransform Pose = RigidActor->getGlobalPose();
	return PhysXConvert::ToFTransform(Pose);
}

void FBodyInstance::SetBodyTransform(const FTransform& WorldTransform)
{
	if (!RigidActor) return;

	RigidActor->setGlobalPose(PhysXConvert::ToPxTransform(WorldTransform));
}

void FBodyInstance::SetKinematicTarget(const FTransform& WorldTransform)
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->setKinematicTarget(PhysXConvert::ToPxTransform(WorldTransform));
}

// 지속적인 힘(dt영향)
void FBodyInstance::AddForce(const FVector& Force)
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->addForce(PhysXConvert::ToPxVec3(Force), physx::PxForceMode::eFORCE);
}

// 순간 충력
void FBodyInstance::AddImpulse(const FVector& Impulse)
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->addForce(PhysXConvert::ToPxVec3(Impulse), physx::PxForceMode::eIMPULSE);
}

// PhysX dynamic actor는 가만히 있으면 sleep상태가 될 수 있다함 -> 깨워줘야함
void FBodyInstance::WakeUp()
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->wakeUp();
}

void FBodyInstance::SetLinearVelocity(const FVector& Velocity) 
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->setLinearVelocity(PhysXConvert::ToPxVec3(Velocity));
}

FVector FBodyInstance::GetLinearVelocity() const
{
	const physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return FVector::ZeroVector;

	return PhysXConvert::ToFVector(Dynamic->getLinearVelocity());
}

void FBodyInstance::ClearPhysicsPointers()
{
	RigidActor = nullptr;
	Shapes.clear();
}
