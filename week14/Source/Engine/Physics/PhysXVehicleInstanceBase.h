#pragma once

#include "Physics/PhysXInclude.h"

class FPhysXVehicleInstanceBase
{
public:
	virtual ~FPhysXVehicleInstanceBase() = default;

	virtual void Shutdown() = 0;
	virtual physx::PxVehicleWheels* GetPxVehicle() const = 0;
	virtual physx::PxRigidDynamic* GetActor() const = 0;
};
