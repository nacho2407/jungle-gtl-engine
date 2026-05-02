#pragma once

#include "MovementComponent.h"
#include "Math/Vector.h"

class UPrimitiveComponent;

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

	float GetForwardSpeed() const;

private:
	void ApplyDriveForce(float DeltaTime);
	void ApplySteering(float DeltaTime);
	void ApplyLateralGrip(float DeltaTime);

	FVector GetPlaneNormal() const;

private:
	UPrimitiveComponent* UpdatedPrimitive = nullptr;

	float ThrottleInput = 0.0f;
	float SteeringInput = 0.0f;

	float MaxSpeed = 20.0f;
	float ReverseMaxSpeed = -15.0f;

	float AccelForce = 15.0f;
	float ReverseAccelForce = 10.0f;
	float BrakeForce = 40.0f;

	float SteeringPower = 90.0f;
	float LateralGrip = 8.0f;
	float RollingDrag = 2.0f;
};
