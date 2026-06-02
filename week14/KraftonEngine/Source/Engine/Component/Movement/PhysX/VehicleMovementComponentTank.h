#pragma once

#include "Component/Movement/PhysX/PhysXVehicleMovementComponent.h"
#include "Math/Quat.h"
#include "Physics/PhysXVehicleTankInstance.h"

#include "Source/Engine/Component/Movement/PhysX/VehicleMovementComponentTank.generated.h"

class USceneComponent;

USTRUCT()
struct FTankVehicleVisualSetup
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Auto Bind Wheel Visuals")
	bool bAutoBindWheelVisuals = true;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Left Wheel Name Prefix")
	FString LeftWheelNamePrefix = "TankWheel_L_";

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Right Wheel Name Prefix")
	FString RightWheelNamePrefix = "TankWheel_R_";

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Wheel Roll Axis", Type = Vec3, Speed = 0.01f)
	FVector WheelRollAxis = FVector(0.0f, 1.0f, 0.0f);

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Left Wheel Rotation Scale", Speed = 0.01f)
	float LeftWheelRotationScale = 1.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Right Wheel Rotation Scale", Speed = 0.01f)
	float RightWheelRotationScale = 1.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Suspension Scale", Speed = 0.01f)
	float SuspensionScale = 1.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Auto Bind Turret Visual")
	bool bAutoBindTurretVisual = true;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Turret Component Name")
	FString TurretComponentName = "TankTurret";

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Turret Yaw Axis", Type = Vec3, Speed = 0.01f)
	FVector TurretYawAxis = FVector::UpVector;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Turret Yaw Speed", Speed = 1.0f)
	float TurretYawSpeedDegPerSecond = 60.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Limit Turret Yaw")
	bool bLimitTurretYaw = false;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Min Turret Yaw", Speed = 1.0f)
	float MinTurretYawDegrees = -180.0f;

	UPROPERTY(Edit, Save, Category = "Tank|Visual", DisplayName = "Max Turret Yaw", Speed = 1.0f)
	float MaxTurretYawDegrees = 180.0f;
};

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

	UFUNCTION(Lua)
	void SetTurretInput(float YawInput);

	UFUNCTION(Lua)
	void SetTurretYaw(float YawDegrees);

	UFUNCTION(Lua)
	float GetTurretYaw() const;

	UFUNCTION(Lua)
	FVector GetTurretForward() const;

	UFUNCTION(Lua)
	void FireTurretRecoil(float Impulse, FVector TurretLocalFirePoint, FVector TurretLocalDirection) const;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void SyncFromPhysics() override;

	UFUNCTION(Lua)
	float GetLeftTrackSpeed() const;

	UFUNCTION(Lua)
	float GetRightTrackSpeed() const;

	UFUNCTION(Lua)
	float GetWheelRotationAngle(int32 WheelIndex) const;

	UFUNCTION(Lua)
	float GetWheelRotationSpeed(int32 WheelIndex) const;

	UFUNCTION(Lua)
	int32 GetWheelCount() const;

protected:
	bool CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene, physx::PxMaterial* Material, const physx::PxTransform& StartPose) override;
	void DestroyVehicleInstance() override;
	physx::PxVehicleWheels* GetPxVehicle() const override;
	physx::PxRigidDynamic* GetVehicleActor() const override;

private:
	struct FWheelVisualBinding
	{
		USceneComponent* Component = nullptr;
		FVector InitialRelativeLocation = FVector::ZeroVector;
		FQuat InitialRelativeRotation;
		uint32 WheelIndex = 0;
	};

	void RebuildVisualBindings();
	void BindWheelVisualByName(const FString& ComponentName, uint32 WheelIndex);
	void BindTurretVisualByName(const FString& ComponentName);
	USceneComponent* FindOwnerSceneComponentByName(const FString& ComponentName) const;
	void ApplyWheelVisuals();
	void ApplyTurretVisual();
	void ClearVisualBindings();
	void UpdateTurretYaw(float DeltaTime);
	float ClampTurretYaw(float YawDegrees) const;
	FQuat GetTurretYawRotation() const;
	FVector GetTurretYawAxis() const;
	bool GetTurretLocalFireTransform(FVector TurretLocalPoint, FVector TurretLocalDirection,
		FVector& OutVehicleLocalPoint, FVector& OutVehicleLocalDirection) const;

	FPhysXVehicleTankInstance* VehicleInstance = nullptr;
	TArray<FWheelVisualBinding> WheelVisualBindings;
	USceneComponent* TurretVisualComponent = nullptr;
	FQuat InitialTurretRelativeRotation;
	float TurretYawInput = 0.0f;
	float TurretYawDegrees = 0.0f;

	UPROPERTY(Edit, Save, Category = "Tank", DisplayName = "Tank Setup", Type = Struct, Struct = FTankVehiclePhysicsSetup)
	FTankVehiclePhysicsSetup TankSetup;

	UPROPERTY(Edit, Save, Category = "Tank", DisplayName = "Visual Setup", Type = Struct, Struct = FTankVehicleVisualSetup)
	FTankVehicleVisualSetup VisualSetup;
};
