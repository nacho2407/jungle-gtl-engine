#pragma once

#include "Physics/PhysXInclude.h"
#include "Physics/PhysXVehicleInstanceBase.h"

#include "Math/Vector.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Physics/PhysXVehicleTankInstance.generated.h"

#include <PhysX/vehicle/PxVehicleDriveTank.h>
#include <PhysX/vehicle/PxVehicleUtilSetup.h>

USTRUCT()
struct FTankVehiclePhysicsSetup
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category = "Tank|Chassis", DisplayName = "Chassis Mass", Min = 1.0f, Speed = 100.0f)
	float ChassisMass = 45000.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Chassis", DisplayName = "Chassis Half Extent", Type = Vec3, Min = 0.01f, Speed = 0.1f)
	FVector ChassisHalfExtent = FVector(3.4f, 1.45f, 0.75f);

	UPROPERTY(Edit, Save, Category = "Tank|Chassis", DisplayName = "Center Of Mass Offset", Type = Vec3, Speed = 0.1f)
	FVector CenterOfMassOffset = FVector(0.0f, 0.0f, -0.55f);

	UPROPERTY(Edit, Save, Category = "Tank|Chassis", DisplayName = "Linear Damping", Min = 0.0f, Speed = 0.01f)
	float LinearDamping = 0.1f;

	UPROPERTY(Edit, Save, Category = "Tank|Chassis", DisplayName = "Angular Damping", Min = 0.0f, Speed = 0.01f)
	float AngularDamping = 0.35f;

	UPROPERTY(Edit, Save, Category = "Tank|Engine", DisplayName = "Engine Peak Torque", Min = 0.0f, Speed = 100.0f)
	float EnginePeakTorque = 65000.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Engine", DisplayName = "Engine Max Omega", Min = 0.0f, Speed = 10.0f)
	float EngineMaxOmega = 800.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Engine", DisplayName = "Clutch Strength", Min = 0.0f, Speed = 1.0f)
	float ClutchStrength = 80.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Track", DisplayName = "Track Half Width", Min = 0.01f, Speed = 0.1f)
	float TrackHalfWidth = 1.35f;

	UPROPERTY(Edit, Save, Category = "Tank|Track", DisplayName = "Road Wheel Count Per Side", Min = 2, Max = 10, Speed = 1.0f)
	int32 RoadWheelCountPerSide = 6;

	UPROPERTY(Edit, Save, Category = "Tank|Track", DisplayName = "Front Wheel Offset X", Speed = 0.1f)
	float FrontWheelOffsetX = 2.1f;

	UPROPERTY(Edit, Save, Category = "Tank|Track", DisplayName = "Rear Wheel Offset X", Speed = 0.1f)
	float RearWheelOffsetX = -2.1f;

	UPROPERTY(Edit, Save, Category = "Tank|Wheel", DisplayName = "Wheel Radius", Min = 0.01f, Speed = 0.01f)
	float WheelRadius = 0.38f;

	UPROPERTY(Edit, Save, Category = "Tank|Wheel", DisplayName = "Wheel Width", Min = 0.01f, Speed = 0.01f)
	float WheelWidth = 0.32f;

	UPROPERTY(Edit, Save, Category = "Tank|Wheel", DisplayName = "Wheel Mass", Min = 0.01f, Speed = 1.0f)
	float WheelMass = 80.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Wheel", DisplayName = "Wheel Damping Rate", Min = 0.0f, Speed = 0.01f)
	float WheelDampingRate = 1.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Wheel", DisplayName = "Max Brake Torque", Min = 0.0f, Speed = 100.0f)
	float MaxBrakeTorque = 8000.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Suspension", DisplayName = "Max Compression", Min = 0.0f, Speed = 0.01f)
	float SuspensionMaxCompression = 0.35f;

	UPROPERTY(Edit, Save, Category = "Tank|Suspension", DisplayName = "Max Droop", Min = 0.0f, Speed = 0.01f)
	float SuspensionMaxDroop = 0.15f;

	UPROPERTY(Edit, Save, Category = "Tank|Suspension", DisplayName = "Spring Strength", Min = 0.0f, Speed = 100.0f)
	float SuspensionSpringStrength = 90000.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Suspension", DisplayName = "Spring Damper Rate", Min = 0.0f, Speed = 100.0f)
	float SuspensionSpringDamperRate = 9000.0f;
};

class FPhysXVehicleTankInstance : public FPhysXVehicleInstanceBase
{
public:
	bool Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene,
		physx::PxMaterial* Material, const physx::PxTransform& StartPose,
		const FTankVehiclePhysicsSetup& Setup);
	void Shutdown() override;

	void SetDriveInput(float Throttle, float Brake, float Steer, bool bReverse);
	void SetTrackInput(float LeftThrust, float RightThrust, float LeftBrake, float RightBrake);
	void FireRecoil(float Impulse, const FVector& LocalFirePoint, const FVector& LocalDirection);
	void UpdateVisualState(const physx::PxVehicleWheelQueryResult* WheelQueryResult = nullptr);

	physx::PxVehicleWheels* GetPxVehicle() const override { return Vehicle; }
	physx::PxRigidDynamic* GetActor() const override { return VehicleActor; }

	float GetLeftTrackSpeed() const { return LeftTrackSpeed; }
	float GetRightTrackSpeed() const { return RightTrackSpeed; }
	float GetWheelRotationAngle(uint32 WheelIndex) const;
	float GetWheelRotationSpeed(uint32 WheelIndex) const;
	float GetWheelSuspensionOffset(uint32 WheelIndex) const;
	uint32 GetWheelCount() const { return WheelCount; }

private:
	physx::PxRigidDynamic* VehicleActor = nullptr;
	physx::PxVehicleDriveTank* Vehicle = nullptr;

	uint32 WheelCount = 0;
	float WheelRadius = 0.0f;
	float LeftTrackSpeed = 0.0f;
	float RightTrackSpeed = 0.0f;
	TArray<float> WheelRestLocalZ;
	TArray<float> WheelRotationAngles;
	TArray<float> WheelRotationSpeeds;
	TArray<float> WheelSuspensionOffsets;
};
