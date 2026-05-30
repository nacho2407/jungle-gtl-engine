#pragma once

#include "Physics/PhysXInclude.h"
#include <PhysX/vehicle/PxVehicleDrive4W.h>
#include <PhysX/vehicle/PxVehicleUtilSetup.h>

class FPhysXVehicleInstance
{
public:
	bool Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene,
		physx::PxMaterial* Material, const physx::PxTransform& StartPose);
	void Shutdown();

	void SetDriveInput(float Throttle, float Brake, float Steer);

	physx::PxVehicleWheels* GetPxVehicle() const { return Vehicle; }
	physx::PxRigidDynamic* GetActor() const { return VehicleActor; }

private:
	physx::PxRigidDynamic* VehicleActor = nullptr;
	physx::PxVehicleDrive4W* Vehicle = nullptr;
};
