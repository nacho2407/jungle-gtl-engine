#pragma once

#include "Physics/PhysXInclude.h"
#include "Physics/PhysXVehicleInstanceBase.h"

#include "Core/Types/CollisionTypes.h"
#include "Math/Vector.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Physics/PhysXVehicle4WInstance.generated.h"

#include <PhysX/vehicle/PxVehicleDrive4W.h>
#include <PhysX/vehicle/PxVehicleUtilSetup.h>

UENUM()
enum class EVehicleDifferential4WType : uint8
{
	LimitedSlip4W,
	LimitedSlipFrontDrive,
	LimitedSlipRearDrive,
	Open4W,
	OpenFrontDrive,
	OpenRearDrive,
};

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

	UPROPERTY(Edit, Save, Category = "Wheel|Suspension", DisplayName = "Max Compression", Min = 0.0f, Speed = 0.01f)
	float SuspensionMaxCompression = 0.3f;

	UPROPERTY(Edit, Save, Category = "Wheel|Suspension", DisplayName = "Max Droop", Min = 0.0f, Speed = 0.01f)
	float SuspensionMaxDroop = 0.1f;

	UPROPERTY(Edit, Save, Category = "Wheel|Suspension", DisplayName = "Spring Strength", Min = 0.0f, Speed = 100.0f)
	float SuspensionSpringStrength = 35000.0f;

	UPROPERTY(Edit, Save, Category = "Wheel|Suspension", DisplayName = "Spring Damper Rate", Min = 0.0f, Speed = 100.0f)
	float SuspensionSpringDamperRate = 4500.0f;
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

	UPROPERTY(Edit, Save, Category = "Vehicle|Chassis", DisplayName = "Linear Damping", Min = 0.0f, Speed = 0.01f)
	float LinearDamping = 0.1f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Chassis", DisplayName = "Angular Damping", Min = 0.0f, Speed = 0.01f)
	float AngularDamping = 0.5f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Collision", DisplayName = "Collision Preset", Enum = ECollisionPreset)
	ECollisionPreset CollisionPreset = ECollisionPreset::PhysicsActor;

	UPROPERTY(Edit, Save, Category = "Vehicle|Collision", DisplayName = "Generate Overlap Events", EditCondition = "CollisionPreset == Custom")
	bool bGenerateOverlapEvents = false;

	UPROPERTY(Edit, Save, Category = "Vehicle|Collision", DisplayName = "Collision Enabled", Enum = ECollisionEnabled, EditCondition = "CollisionPreset == Custom")
	ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	UPROPERTY(Edit, Save, Category = "Vehicle|Collision", DisplayName = "Object Type", Enum = ECollisionChannel, EditCondition = "CollisionPreset == Custom")
	ECollisionChannel ObjectType = ECollisionChannel::WorldDynamic;

	UPROPERTY(Edit, Save, Category = "Vehicle|Collision", DisplayName = "Collision Responses", Type = Struct, Struct = FCollisionResponseContainer, EditCondition = "CollisionPreset == Custom")
	FCollisionResponseContainer ResponseContainer;

	UPROPERTY(Edit, Save, Category = "Vehicle|Engine", DisplayName = "Engine Peak Torque", Min = 0.0f, Speed = 10.0f)
	float EnginePeakTorque = 500.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Engine", DisplayName = "Engine Max Omega", Min = 0.0f, Speed = 10.0f)
	float EngineMaxOmega = 600.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Engine", DisplayName = "Clutch Strength", Min = 0.0f, Speed = 1.0f)
	float ClutchStrength = 10.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Drive", DisplayName = "Use Auto Gears")
	bool bUseAutoGears = true;

	UPROPERTY(Edit, Save, Category = "Vehicle|Drive", DisplayName = "Differential Type", Enum = EVehicleDifferential4WType)
	EVehicleDifferential4WType DifferentialType = EVehicleDifferential4WType::LimitedSlip4W;

	UPROPERTY(Edit, Save, Category = "Vehicle|Force", DisplayName = "Force App Point Z Offset", Speed = 0.01f)
	float ForceAppPointZOffset = -0.3f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Ackermann", DisplayName = "Auto Ackermann From Wheel Offsets")
	bool bAutoAckermannFromWheelOffsets = true;

	UPROPERTY(Edit, Save, Category = "Vehicle|Ackermann", DisplayName = "Ackermann Accuracy", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float AckermannAccuracy = 1.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Ackermann", DisplayName = "Axle Separation", Min = 0.0f, Speed = 0.1f)
	float AckermannAxleSeparation = 3.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Ackermann", DisplayName = "Front Width", Min = 0.0f, Speed = 0.1f)
	float AckermannFrontWidth = 2.0f;

	UPROPERTY(Edit, Save, Category = "Vehicle|Ackermann", DisplayName = "Rear Width", Min = 0.0f, Speed = 0.1f)
	float AckermannRearWidth = 2.0f;

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
