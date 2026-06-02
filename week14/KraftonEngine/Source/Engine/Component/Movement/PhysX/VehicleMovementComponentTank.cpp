#include "VehicleMovementComponentTank.h"

void UVehicleMovementComponentTank::SetDriveInput(const float Throttle, const float Brake, const float Steer, const bool bReverse) const
{
	if (VehicleInstance)
	{
		VehicleInstance->SetDriveInput(Throttle, Brake, Steer, bReverse);
	}
}

void UVehicleMovementComponentTank::SetTrackInput(const float LeftThrust, const float RightThrust,
	const float LeftBrake, const float RightBrake) const
{
	if (VehicleInstance)
	{
		VehicleInstance->SetTrackInput(LeftThrust, RightThrust, LeftBrake, RightBrake);
	}
}

void UVehicleMovementComponentTank::FireRecoil(const float Impulse, const FVector LocalFirePoint, const FVector LocalDirection) const
{
	if (VehicleInstance)
	{
		VehicleInstance->FireRecoil(Impulse, LocalFirePoint, LocalDirection);
	}
}

void UVehicleMovementComponentTank::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UPhysXVehicleMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (VehicleInstance)
	{
		VehicleInstance->UpdateVisualState();
	}
}

float UVehicleMovementComponentTank::GetLeftTrackSpeed() const
{
	return VehicleInstance ? VehicleInstance->GetLeftTrackSpeed() : 0.0f;
}

float UVehicleMovementComponentTank::GetRightTrackSpeed() const
{
	return VehicleInstance ? VehicleInstance->GetRightTrackSpeed() : 0.0f;
}

float UVehicleMovementComponentTank::GetWheelRotationAngle(const uint32 WheelIndex) const
{
	return VehicleInstance ? VehicleInstance->GetWheelRotationAngle(WheelIndex) : 0.0f;
}

float UVehicleMovementComponentTank::GetWheelRotationSpeed(const uint32 WheelIndex) const
{
	return VehicleInstance ? VehicleInstance->GetWheelRotationSpeed(WheelIndex) : 0.0f;
}

uint32 UVehicleMovementComponentTank::GetWheelCount() const
{
	return VehicleInstance ? VehicleInstance->GetWheelCount() : 0;
}

bool UVehicleMovementComponentTank::CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene,
	physx::PxMaterial* Material, const physx::PxTransform& StartPose)
{
	DestroyVehicleInstance();

	VehicleInstance = new FPhysXVehicleTankInstance();
	if (!VehicleInstance->Initialize(Physics, Scene, Material, StartPose, TankSetup))
	{
		DestroyVehicleInstance();
		return false;
	}

	return true;
}

void UVehicleMovementComponentTank::DestroyVehicleInstance()
{
	if (VehicleInstance)
	{
		VehicleInstance->Shutdown();
		delete VehicleInstance;
		VehicleInstance = nullptr;
	}
}

physx::PxVehicleWheels* UVehicleMovementComponentTank::GetPxVehicle() const
{
	return VehicleInstance ? VehicleInstance->GetPxVehicle() : nullptr;
}

physx::PxRigidDynamic* UVehicleMovementComponentTank::GetVehicleActor() const
{
	return VehicleInstance ? VehicleInstance->GetActor() : nullptr;
}
