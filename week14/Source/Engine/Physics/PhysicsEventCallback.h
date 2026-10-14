#pragma once

#include "Physics/PhysXInclude.h"

class FPhysicsEventCallback : public physx::PxSimulationEventCallback
{
public:
	void onContact(const physx::PxContactPairHeader& PairHeader,
		const physx::PxContactPair* Pairs, physx::PxU32 Count) override;
	void onTrigger(physx::PxTriggerPair* Pairs, physx::PxU32 Count) override;

	void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override {}
	void onWake(physx::PxActor**, physx::PxU32) override {}
	void onSleep(physx::PxActor**, physx::PxU32) override {}
	void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) override {}
};
