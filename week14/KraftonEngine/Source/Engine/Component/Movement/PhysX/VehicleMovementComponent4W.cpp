#include "VehicleMovementComponent4W.h"

void UVehicleMovementComponent4W::SetDriveInput(const float Throttle, const float Brake, const float Steer, const bool bReverse) const
{
	if (VehicleInstance)
	{
		VehicleInstance->SetDriveInput(Throttle, Brake, Steer, bReverse);
	}
}

bool UVehicleMovementComponent4W::CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene,
	physx::PxMaterial* Material, const physx::PxTransform& StartPose)
{
	DestroyVehicleInstance();

	VehicleInstance = new FPhysXVehicle4WInstance();
	if (!VehicleInstance->Initialize(Physics, Scene, Material, StartPose, VehicleSetup))
	{
		DestroyVehicleInstance();
		return false;
	}

	return true;
}

void UVehicleMovementComponent4W::DestroyVehicleInstance()
{
	if (VehicleInstance)
	{
		VehicleInstance->Shutdown();
		delete VehicleInstance;
		VehicleInstance = nullptr;
	}
}

physx::PxVehicleWheels* UVehicleMovementComponent4W::GetPxVehicle() const
{
	return VehicleInstance ? VehicleInstance->GetPxVehicle() : nullptr;
}

physx::PxRigidDynamic* UVehicleMovementComponent4W::GetVehicleActor() const
{
	return VehicleInstance ? VehicleInstance->GetActor() : nullptr;
}
