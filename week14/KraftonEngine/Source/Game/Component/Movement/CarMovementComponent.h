#pragma once

#include "Component/Movement/MovementComponent.h"
#include "Math/Vector.h"

class UPrimitiveComponent;
class USphereComponent;

class UCarMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UCarMovementComponent, UMovementComponent)

	UCarMovementComponent() = default;
	~UCarMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

	void SetThrottleInput(float Value) { ThrottleInput = std::max<float>(-1.0f, std::min<float>(1.0f, Value)); }
	void SetSteeringInput(float Value) { SteeringInput = std::max<float>(-1.0f, std::min<float>(1.0f, Value)); }

	void StopImmediately();
	float GetForwardSpeed() const;

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

	float MaxSpeed = 100.0f;
	float ReverseMaxSpeed = -80.0f;

	float AccelForce = 20000.0f;
	float ReverseAccelForce = 15000.0f;
	float BrakeForce = 50000.0f;

	float SteeringPower = 15000.0f;
	float LateralGrip = 8000.0f;
	float RollingDrag = 800.0f;

	bool bUseRaycastSuspension = true;
	bool bDisableWheelCollision = true;
	float SuspensionRestLength = 0.6f;
	float SuspensionSpringStrength = 12000.0f;
	float SuspensionDamping = 3500.0f;
	float MaxSuspensionForce = 25000.0f;
	float GroundAngularDamping = 8000.0f;
	float WheelRadius = 0.4f;
	float WheelForwardOffset = 1.5f;
	float WheelHalfTrack = 0.8f;
	float WheelRootZ = 0.0f;
	float AirSteeringScale = 0.15f;
};
