#pragma once

#include "Component/Movement/PhysX/PhysXVehicleMovementComponent.h"
#include "Physics/PhysXVehicle4WInstance.h"

#include "Source/Engine/Component/Movement/PhysX/VehicleMovementComponent4W.generated.h"

UCLASS()
class UVehicleMovementComponent4W : public UPhysXVehicleMovementComponent
{
public:
	GENERATED_BODY()

	UVehicleMovementComponent4W() = default;
	~UVehicleMovementComponent4W() override = default;

	UFUNCTION(Lua)
	void SetDriveInput(float Throttle, float Brake, float Steer, bool bReverse) const;

protected:
	bool CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene, physx::PxMaterial* Material, const physx::PxTransform& StartPose) override;
	void DestroyVehicleInstance() override;
	physx::PxVehicleWheels* GetPxVehicle() const override;
	physx::PxRigidDynamic* GetVehicleActor() const override;

private:
	FPhysXVehicle4WInstance* VehicleInstance = nullptr;

	UPROPERTY(Edit, Save, Category = "Vehicle", DisplayName = "Vehicle Setup", Type = Struct, Struct = FVehiclePhysicsSetup)
	FVehiclePhysicsSetup VehicleSetup;
};
