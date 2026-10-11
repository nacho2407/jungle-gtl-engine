#pragma once

#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/Movement/PhysX/PhysXVehicleMovementComponent.h"
#include "Physics/PhysXVehicle4WInstance.h"

#include "Source/Engine/Component/Movement/PhysX/VehicleMovementComponent4W.generated.h"

class FScene;

UCLASS()
class UVehicleMovementComponent4W : public UPhysXVehicleMovementComponent
{
public:
	GENERATED_BODY()

	UVehicleMovementComponent4W() = default;
	~UVehicleMovementComponent4W() override = default;

	UFUNCTION(Lua)
	void SetDriveInput(float Throttle, float Brake, float Steer, bool bReverse) const;

	void ContributeSelectedVisuals(FScene& Scene) const override;

protected:
	bool CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene, physx::PxMaterial* Material, const physx::PxTransform& StartPose) override;
	void DestroyVehicleInstance() override;
	physx::PxVehicleWheels* GetPxVehicle() const override;
	physx::PxRigidDynamic* GetVehicleActor() const override;

private:
	void ApplyWheelMeshSteer(float Steer) const;
	void ApplySingleWheelMeshSteer(UStaticMeshComponent* WheelMesh, FRotator& CachedBaseRotation, bool& bHasCachedBaseRotation, float Steer) const;

	FPhysXVehicle4WInstance* VehicleInstance = nullptr;

	UPROPERTY(Edit, Save, Category = "Vehicle", DisplayName = "Vehicle Setup", Type = Struct, Struct = FVehiclePhysicsSetup)
	FVehiclePhysicsSetup VehicleSetup;

	UPROPERTY(Edit, Save, Category = "Vehicle|Debug", DisplayName = "Show Vehicle Shape")
	bool bShowVehicleShape = true;

	UPROPERTY(Edit, Save, Category = "Vehicle|Visual", DisplayName = "Front Left Wheel Mesh")
	UStaticMeshComponent* FrontLeftWheelMesh = nullptr;

	UPROPERTY(Edit, Save, Category = "Vehicle|Visual", DisplayName = "Front Right Wheel Mesh")
	UStaticMeshComponent* FrontRightWheelMesh = nullptr;

	UPROPERTY(Edit, Save, Category = "Vehicle|Visual", DisplayName = "Rear Left Wheel Mesh")
	UStaticMeshComponent* RearLeftWheelMesh = nullptr;

	UPROPERTY(Edit, Save, Category = "Vehicle|Visual", DisplayName = "Rear Right Wheel Mesh")
	UStaticMeshComponent* RearRightWheelMesh = nullptr;

	UPROPERTY(Edit, Save, Category = "Vehicle|Visual", DisplayName = "Visual Steer Rotation Scale", Type = Rotator, Speed = 0.5f)
	FRotator VisualSteerRotationScale = FRotator(0.0f, 30.0f, 0.0f);

	mutable bool bHasFrontLeftWheelBaseRotation = false;
	mutable bool bHasFrontRightWheelBaseRotation = false;
	mutable FRotator FrontLeftWheelBaseRotation;
	mutable FRotator FrontRightWheelBaseRotation;
};
