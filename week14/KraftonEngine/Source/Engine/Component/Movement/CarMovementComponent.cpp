#include "CarMovementComponent.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UCarMovementComponent, UMovementComponent)

namespace
{
	FVector ProjectOnPlane(const FVector& Vector, const FVector& PlaneNormal)
	{
		return Vector - PlaneNormal * Vector.Dot(PlaneNormal);
	}
}

void UCarMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
}

void UCarMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
		UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!UpdatedPrimitive) return;

	ApplyDriveForce(DeltaTime);
	ApplySteering(DeltaTime);
	ApplyLateralGrip(DeltaTime);
}

void UCarMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back(FPropertyDescriptor("MaxSpeed", EPropertyType::Float, "Movement", &MaxSpeed));
	OutProps.push_back(FPropertyDescriptor("ReverseMaxSpeed", EPropertyType::Float, "Movement", &ReverseMaxSpeed));
	OutProps.push_back(FPropertyDescriptor("AccelForce", EPropertyType::Float, "Movement", &AccelForce));
	OutProps.push_back(FPropertyDescriptor("ReverseAccelForce", EPropertyType::Float, "Movement", &ReverseAccelForce));
	OutProps.push_back(FPropertyDescriptor("BrakeForce", EPropertyType::Float, "Movement", &BrakeForce));
	OutProps.push_back(FPropertyDescriptor("SteeringPower", EPropertyType::Float, "Movement", &SteeringPower));
	OutProps.push_back(FPropertyDescriptor("LateralGrip", EPropertyType::Float, "Movement", &LateralGrip));
	OutProps.push_back(FPropertyDescriptor("RollingDrag", EPropertyType::Float, "Movement", &RollingDrag));
}

void UCarMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << MaxSpeed;
	Ar << ReverseMaxSpeed;
	Ar << AccelForce;
	Ar << ReverseAccelForce;
	Ar << BrakeForce;
	Ar << SteeringPower;
	Ar << LateralGrip;
	Ar << RollingDrag;
}

float UCarMovementComponent::GetForwardSpeed() const
{
	if (!UpdatedPrimitive) return 0.0f;
	FVector Forward = UpdatedPrimitive->GetForwardVector();
	FVector Velocity = UpdatedPrimitive->GetLinearVelocity();

	return Forward.Dot(Velocity);
}

void UCarMovementComponent::ApplyDriveForce(float DeltaTime) {
	if (!UpdatedPrimitive) return;

	FVector Forward = UpdatedPrimitive->GetForwardVector();
	float ForwardSpeed = GetForwardSpeed();

	if (ThrottleInput > 0.0f)
	{
		if (ForwardSpeed < 0.0f)
		{
			UpdatedPrimitive->AddForce(Forward * BrakeForce);
		}
		else if (ForwardSpeed < MaxSpeed)
		{
			UpdatedPrimitive->AddForce(Forward * AccelForce);
		}
	}
	else if (ThrottleInput < 0.0f)
	{
		if (ForwardSpeed > 0.0f)
		{
			UpdatedPrimitive->AddForce(Forward * -BrakeForce);
		}
		else if (ForwardSpeed > ReverseMaxSpeed)
		{
			UpdatedPrimitive->AddForce(Forward * -ReverseAccelForce);
		}
	}
	else
	{
		UpdatedPrimitive->AddForce(Forward * -ForwardSpeed * RollingDrag);
	}
}

void UCarMovementComponent::ApplySteering(float DeltaTime) {
	if (!UpdatedPrimitive) return;

	float ForwardSpeed = GetForwardSpeed();
	float SpeedFactor = FMath::Clamp(std::abs(ForwardSpeed) / MaxSpeed, 0.35f, 1.0f);

	float Torque = SteeringInput * SteeringPower * SpeedFactor;

	if (ForwardSpeed < 0.0f)
	{
		Torque = -Torque;
	}

	UpdatedPrimitive->AddTorque(FVector(0.0f, 0.0f, Torque));
}

void UCarMovementComponent::ApplyLateralGrip(float DeltaTime) {
	if (!UpdatedPrimitive) return;

	FVector Right = UpdatedPrimitive->GetRightVector();
	FVector Velocity = UpdatedPrimitive->GetLinearVelocity();
	float LateralSpeed = Velocity.Dot(Right);

	UpdatedPrimitive->AddForce(Right * -LateralSpeed * LateralGrip);
}

FVector UCarMovementComponent::GetPlaneNormal() const
{
	if (!UpdatedPrimitive) return FVector::UpVector;

	FVector Forward = UpdatedPrimitive->GetForwardVector();
	FVector Right = UpdatedPrimitive->GetRightVector();
	return Forward.Cross(Right).Normalized();
}
