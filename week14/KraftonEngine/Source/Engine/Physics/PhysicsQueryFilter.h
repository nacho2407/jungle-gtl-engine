#pragma once

#include "Physics/PhysXInclude.h"
#include "Core/Types/CollisionTypes.h"

class AActor;
class UPrimitiveComponent;

UPrimitiveComponent* GetComponentFromQueryShape(const physx::PxShape* Shape);

class FPhysicsRaycastFilterCallback : public physx::PxQueryFilterCallback
{
public:
	FPhysicsRaycastFilterCallback(ECollisionChannel TraceChannel, const AActor* IgnoreActor);

	virtual physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& FilterData, const physx::PxShape* Shape,
		const physx::PxRigidActor* Actor, physx::PxHitFlags& QueryFlags) override;
	virtual physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& FilterData, const physx::PxQueryHit& Hit) override;

private:
	ECollisionChannel TraceChannel;
	const AActor* IgnoreActor;
};
