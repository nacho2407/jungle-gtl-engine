#pragma once

#include "Physics/PhysXInclude.h"
#include "Physics/PhysXVehicleInstanceBase.h"

#include "Math/Vector.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Physics/PhysXVehicle4WInstance.generated.h"

#include <PhysX/vehicle/PxVehicleDrive4W.h>
#include <PhysX/vehicle/PxVehicleUtilSetup.h>

USTRUCT()
struct FVehicleWheelSetup
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category = "Wheel", DisplayName = "Offset", Type = Vec3, Speed = 0.1f)
	FVector Offset;

	UPROPERTY(Edit, Save, Category = "Wheel", DisplayName = "Radius", Min = 0.01f, Speed = 0.01f)
	float Radius = 0.35f;

	UPROPERTY(Edit, Save, Category = "Wheel", DisplayName = "Width", Min = 0.01f, Speed = 0.01f)
	float Width = 0.25f;

	UPROPERTY(Edit, Save, Category = "Wheel", DisplayName = "Mass", Min = 0.01f, Speed = 1.0f)
	float Mass = 20.0f;

	UPROPERTY(Edit, Save, Category = "Wheel", DisplayName = "Damping Rate", Min = 0.0f, Speed = 0.01f)
	float DampingRate = 0.25f;

	UPROPERTY(Edit, Save, Category = "Wheel", DisplayName = "Max Steer", Min = 0.0f, Speed = 0.01f)
	float MaxSteer = 0.0f;

	UPROPERTY(Edit, Save, Category = "Wheel", DisplayName = "Max Brake Torque", Min = 0.0f, Speed = 10.0f)
	float MaxBrakeTorque = 1500.0f;

	UPROPERTY(Edit, Save, Category = "Wheel", DisplayName = "Max Hand Brake Torque", Min = 0.0f, Speed = 10.0f)
	float MaxHandBrakeTorque = 0.0f;
};

USTRUCT()
struct FVehiclePhysicsSetup
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category = "Vehicle|Chassis", DisplayName = "Chassis Mass", Min = 1.0f, Speed = 10.0f)
	float ChassisMass = 1200.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Chassis", DisplayName = "Chassis Half Extent", Type = Vec3, Min = 0.01f, Speed = 0.1f)
	FVector ChassisHalfExtent = FVector(2.4f, 1.2f, 0.5f);

	UPROPERTY(Edit, Save, Category = "Vehicle|Chassis", DisplayName = "Center Of Mass Offset", Type = Vec3, Speed = 0.1f)
	FVector CenterOfMassOffset = FVector(0.0f, 0.0f, -0.3f);

	UPROPERTY(Edit, Save, Category = "Vehicle|Engine", DisplayName = "Engine Peak Torque", Min = 0.0f, Speed = 10.0f)
	float EnginePeakTorque = 500.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Engine", DisplayName = "Engine Max Omega", Min = 0.0f, Speed = 10.0f)
	float EngineMaxOmega = 600.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Engine", DisplayName = "Clutch Strength", Min = 0.0f, Speed = 1.0f)
	float ClutchStrength = 10.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Wheels", DisplayName = "Front Left Wheel", Type = Struct, Struct = FVehicleWheelSetup)
	FVehicleWheelSetup FrontLeftWheel;

	UPROPERTY(Edit, Save, Category = "Vehicle|Wheels", DisplayName = "Front Right Wheel", Type = Struct, Struct = FVehicleWheelSetup)
	FVehicleWheelSetup FrontRightWheel;

	UPROPERTY(Edit, Save, Category = "Vehicle|Wheels", DisplayName = "Rear Left Wheel", Type = Struct, Struct = FVehicleWheelSetup)
	FVehicleWheelSetup RearLeftWheel;

	UPROPERTY(Edit, Save, Category = "Vehicle|Wheels", DisplayName = "Rear Right Wheel", Type = Struct, Struct = FVehicleWheelSetup)
	FVehicleWheelSetup RearRightWheel;
};

class FPhysXVehicle4WInstance : public FPhysXVehicleInstanceBase
{
public:
	bool Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene,
		physx::PxMaterial* Material, const physx::PxTransform& StartPose,
		const FVehiclePhysicsSetup& Setup);
	void Shutdown() override;

	void SetDriveInput(float Throttle, float Brake, float Steer, bool bReverse);

	physx::PxVehicleWheels* GetPxVehicle() const override { return Vehicle; }
	physx::PxRigidDynamic* GetActor() const override { return VehicleActor; }

private:
	physx::PxRigidDynamic* VehicleActor = nullptr;
	physx::PxVehicleDrive4W* Vehicle = nullptr;

	bool bIsReverse = false;
};
