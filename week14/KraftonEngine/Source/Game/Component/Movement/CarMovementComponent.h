#pragma once

#include "Component/Movement/MovementComponent.h"
#include "Math/Vector.h"

#include "Source/Game/Component/Movement/CarMovementComponent.generated.h"
class UPrimitiveComponent;
class USphereComponent;

UCLASS()
class UCarMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()
	UCarMovementComponent() = default;
	~UCarMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void SetThrottleInput(float Value) { ThrottleInput = std::max<float>(-1.0f, std::min<float>(1.0f, Value)); }
	void SetSteeringInput(float Value) { SteeringInput = std::max<float>(-1.0f, std::min<float>(1.0f, Value)); }

	void StopImmediately();
	float GetForwardSpeed() const;
	float GetMaxSpeed() const { return MaxSpeed; }

private:
	bool ApplyWheelSuspension(float DeltaTime);
	void ApplyAirSteering(float DeltaTime);
	void ApplyRigidBodyMovement(float DeltaTime);
	void CacheWheelComponents();

	FVector GetPlaneNormal() const;

private:
	UPrimitiveComponent* UpdatedPrimitive = nullptr;
	TArray<USphereComponent*> WheelComponents;

	float ThrottleInput = 0.0f;
	float SteeringInput = 0.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="MaxSpeed", Min=0.0f, Max=200.0f, Speed=0.5f)
	float MaxSpeed = 100.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="ReverseMaxSpeed", Min=-200.0f, Max=0.0f, Speed=0.5f)
	float ReverseMaxSpeed = -80.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="AccelForce", Min=0.0f, Max=5000.0f, Speed=10.0f)
	float AccelForce = 20000.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="ReverseAccelForce", Min=0.0f, Max=5000.0f, Speed=10.0f)
	float ReverseAccelForce = 15000.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="BrakeForce", Min=0.0f, Max=10000.0f, Speed=10.0f)
	float BrakeForce = 50000.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="SteeringPower", Min=0.0f, Max=1000.0f, Speed=1.0f)
	float SteeringPower = 15000.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="LateralGrip", Min=0.0f, Max=500.0f, Speed=0.5f)
	float LateralGrip = 8000.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="RollingDrag", Min=0.0f, Max=500.0f, Speed=0.5f)
	float RollingDrag = 800.0f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Use Raycast Suspension")
	bool bUseRaycastSuspension = true;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Disable Wheel Collision")
	bool bDisableWheelCollision = true;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Suspension Rest Length", Min=0.05f, Max=5.0f, Speed=0.01f)
	float SuspensionRestLength = 0.6f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Suspension Spring Strength", Min=0.0f, Max=10000.0f, Speed=10.0f)
	float SuspensionSpringStrength = 12000.0f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Suspension Damping", Min=0.0f, Max=5000.0f, Speed=5.0f)
	float SuspensionDamping = 3500.0f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Max Suspension Force", Min=0.0f, Max=20000.0f, Speed=10.0f)
	float MaxSuspensionForce = 25000.0f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Ground Angular Damping", Min=0.0f, Max=5000.0f, Speed=5.0f)
	float GroundAngularDamping = 8000.0f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Wheel Radius", Min=0.01f, Max=5.0f, Speed=0.01f)
	float WheelRadius = 0.4f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Wheel Forward Offset", Min=0.0f, Max=10.0f, Speed=0.05f)
	float WheelForwardOffset = 1.5f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Wheel Half Track", Min=0.0f, Max=10.0f, Speed=0.05f)
	float WheelHalfTrack = 0.8f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Wheel Root Z", Min=-5.0f, Max=5.0f, Speed=0.05f)
	float WheelRootZ = 0.0f;

	UPROPERTY(Edit, Save, Category="Suspension", DisplayName="Air Steering Scale", Min=0.0f, Max=1.0f, Speed=0.01f)
	float AirSteeringScale = 0.15f;
};
