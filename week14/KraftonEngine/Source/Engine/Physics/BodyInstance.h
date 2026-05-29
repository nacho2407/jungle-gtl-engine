#pragma once

#include "Physics/PhysXInclude.h"

class AActor;
class UPrimitiveComponent;

struct FBodyInstance
{
	UPrimitiveComponent* OwnerComponent = nullptr;
	physx::PxRigidActor* Body = nullptr;

	AActor* GetOwnerActor() const;

	void SyncFromPhysics();
	void SyncToPhysics();
};