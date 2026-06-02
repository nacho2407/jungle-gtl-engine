#pragma once

#include "Component/Movement/PhysX/PhysXVehicleMovementComponent.h"
#include "Physics/PhysXVehicleTankInstance.h"

#include "Source/Engine/Component/Movement/PhysX/VehicleMovementComponentTank.generated.h"

UCLASS()
class UVehicleMovementComponentTank : public UPhysXVehicleMovementComponent
{
public:
	GENERATED_BODY()

	UVehicleMovementComponentTank() = default;
	~UVehicleMovementComponentTank() override = default;

	UFUNCTION(Lua)
	void SetDriveInput(float Throttle, float Brake, float Steer, bool bReverse) const;

	UFUNCTION(Lua)
	void SetTrackInput(float LeftThrust, float RightThrust, float LeftBrake, float RightBrake) const;

	UFUNCTION(Lua)
	void FireRecoil(float Impulse, FVector LocalFirePoint, FVector LocalDirection) const;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	float GetLeftTrackSpeed() const;
	float GetRightTrackSpeed() const;
	float GetWheelRotationAngle(uint32 WheelIndex) const;
	float GetWheelRotationSpeed(uint32 WheelIndex) const;
	uint32 GetWheelCount() const;

protected:
	bool CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene, physx::PxMaterial* Material, const physx::PxTransform& StartPose) override;
	void DestroyVehicleInstance() override;
	physx::PxVehicleWheels* GetPxVehicle() const override;
	physx::PxRigidDynamic* GetVehicleActor() const override;

private:
	FPhysXVehicleTankInstance* VehicleInstance = nullptr;

	UPROPERTY(Edit, Save, Category = "Tank", DisplayName = "Tank Setup", Type = Struct, Struct = FTankVehiclePhysicsSetup)
	FTankVehiclePhysicsSetup TankSetup;
};
