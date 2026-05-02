#include "CarMovementComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SphereComponent.h"
#include "Core/CollisionTypes.h"
#include "Serialization/Archive.h"

#include <algorithm>

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
	CacheWheelComponents();
}

void UCarMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
		UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!UpdatedPrimitive) return;

	if (!bUseRaycastSuspension)
	{
		ApplyRigidBodyMovement(DeltaTime);
		return;
	}

	const bool bHasGroundContact = ApplyWheelSuspension(DeltaTime);
	if (!bHasGroundContact)
	{
		ApplyAirSteering(DeltaTime);
	}
}

void UCarMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "MaxSpeed", EPropertyType::Float, "Movement", &MaxSpeed, 0.0f, 200.0f, 0.5f });
	OutProps.push_back({ "ReverseMaxSpeed", EPropertyType::Float, "Movement", &ReverseMaxSpeed, -200.0f, 0.0f, 0.5f });
	OutProps.push_back({ "AccelForce", EPropertyType::Float, "Movement", &AccelForce, 0.0f, 5000.0f, 10.0f });
	OutProps.push_back({ "ReverseAccelForce", EPropertyType::Float, "Movement", &ReverseAccelForce, 0.0f, 5000.0f, 10.0f });
	OutProps.push_back({ "BrakeForce", EPropertyType::Float, "Movement", &BrakeForce, 0.0f, 10000.0f, 10.0f });
	OutProps.push_back({ "SteeringPower", EPropertyType::Float, "Movement", &SteeringPower, 0.0f, 1000.0f, 1.0f });
	OutProps.push_back({ "LateralGrip", EPropertyType::Float, "Movement", &LateralGrip, 0.0f, 500.0f, 0.5f });
	OutProps.push_back({ "RollingDrag", EPropertyType::Float, "Movement", &RollingDrag, 0.0f, 500.0f, 0.5f });
	OutProps.push_back({ "Use Raycast Suspension", EPropertyType::Bool, "Suspension", &bUseRaycastSuspension });
	OutProps.push_back({ "Disable Wheel Collision", EPropertyType::Bool, "Suspension", &bDisableWheelCollision });
	OutProps.push_back({ "Suspension Rest Length", EPropertyType::Float, "Suspension", &SuspensionRestLength, 0.05f, 5.0f, 0.01f });
	OutProps.push_back({ "Suspension Spring Strength", EPropertyType::Float, "Suspension", &SuspensionSpringStrength, 0.0f, 10000.0f, 10.0f });
	OutProps.push_back({ "Suspension Damping", EPropertyType::Float, "Suspension", &SuspensionDamping, 0.0f, 5000.0f, 5.0f });
	OutProps.push_back({ "Max Suspension Force", EPropertyType::Float, "Suspension", &MaxSuspensionForce, 0.0f, 20000.0f, 10.0f });
	OutProps.push_back({ "Ground Angular Damping", EPropertyType::Float, "Suspension", &GroundAngularDamping, 0.0f, 5000.0f, 5.0f });
	OutProps.push_back({ "Wheel Radius", EPropertyType::Float, "Suspension", &WheelRadius, 0.01f, 5.0f, 0.01f });
	OutProps.push_back({ "Wheel Forward Offset", EPropertyType::Float, "Suspension", &WheelForwardOffset, 0.0f, 10.0f, 0.05f });
	OutProps.push_back({ "Wheel Half Track", EPropertyType::Float, "Suspension", &WheelHalfTrack, 0.0f, 10.0f, 0.05f });
	OutProps.push_back({ "Wheel Root Z", EPropertyType::Float, "Suspension", &WheelRootZ, -5.0f, 5.0f, 0.05f });
	OutProps.push_back({ "Air Steering Scale", EPropertyType::Float, "Suspension", &AirSteeringScale, 0.0f, 1.0f, 0.01f });
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
	Ar << bUseRaycastSuspension;
	Ar << bDisableWheelCollision;
	Ar << SuspensionRestLength;
	Ar << SuspensionSpringStrength;
	Ar << SuspensionDamping;
	Ar << MaxSuspensionForce;
	Ar << GroundAngularDamping;
	Ar << WheelRadius;
	Ar << WheelForwardOffset;
	Ar << WheelHalfTrack;
	Ar << WheelRootZ;
	Ar << AirSteeringScale;
}

float UCarMovementComponent::GetForwardSpeed() const
{
	if (!UpdatedPrimitive) return 0.0f;
	FVector Forward = UpdatedPrimitive->GetForwardVector();
	FVector Velocity = UpdatedPrimitive->GetLinearVelocity();

	return Forward.Dot(Velocity);
}

bool UCarMovementComponent::ApplyWheelSuspension(float DeltaTime)
{
	if (!UpdatedPrimitive) return false;

	FVector Forward = UpdatedPrimitive->GetForwardVector();
	FVector Right = UpdatedPrimitive->GetRightVector();
	FVector Up = UpdatedPrimitive->GetUpVector();
	FVector Down = Up * -1.0f;
	FVector Velocity = UpdatedPrimitive->GetLinearVelocity();
	FVector AngularVelocity = UpdatedPrimitive->GetAngularVelocity();
	FVector BodyLocation = UpdatedPrimitive->GetWorldLocation();

	AActor* OwnerActor = GetOwner();
	UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World) return false;

	const float RayLength = SuspensionRestLength + WheelRadius;
	const float LocalWheelPositions[4][3] = {
		{  WheelForwardOffset,  WheelHalfTrack, WheelRootZ },
		{  WheelForwardOffset, -WheelHalfTrack, WheelRootZ },
		{ -WheelForwardOffset,  WheelHalfTrack, WheelRootZ },
		{ -WheelForwardOffset, -WheelHalfTrack, WheelRootZ },
	};

	int32 GroundedWheelCount = 0;
	const float ForwardSpeed = Forward.Dot(Velocity);
	const int32 WheelCount = WheelComponents.empty() ? 4 : static_cast<int32>(WheelComponents.size());

	for (int32 Index = 0; Index < WheelCount; ++Index)
	{
		FVector WheelRoot = UpdatedPrimitive->GetWorldLocation();
		if (Index < static_cast<int32>(WheelComponents.size()) && WheelComponents[Index])
		{
			WheelRoot = WheelComponents[Index]->GetWorldLocation() + Up * SuspensionRestLength;
		}
		else if (Index < 4)
		{
			WheelRoot = UpdatedPrimitive->GetWorldLocation()
				+ Forward * LocalWheelPositions[Index][0]
				+ Right * LocalWheelPositions[Index][1]
				+ Up * LocalWheelPositions[Index][2];
		}

		FHitResult Hit;
		if (!World->PhysicsRaycast(WheelRoot, Down, RayLength, Hit, OwnerActor))
		{
			continue;
		}

		const FVector ContactPoint = Hit.WorldHitLocation;
		const FVector WheelOffsetFromCenter = WheelRoot - BodyLocation;
		const FVector WheelPointVelocity = Velocity + AngularVelocity.Cross(WheelOffsetFromCenter);
		const float Compression = FMath::Clamp((RayLength - Hit.Distance) / SuspensionRestLength, 0.0f, 1.0f);
		const float SuspensionVelocity = WheelPointVelocity.Dot(Up);
		const float SuspensionForce = FMath::Clamp(Compression * SuspensionSpringStrength - SuspensionVelocity * SuspensionDamping, 0.0f, MaxSuspensionForce);

		UpdatedPrimitive->AddForceAtLocation(Up * SuspensionForce, ContactPoint);

		++GroundedWheelCount;
	}

	if (GroundedWheelCount > 0)
	{
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

		const float LateralSpeed = Velocity.Dot(Right);
		UpdatedPrimitive->AddForce(Right * -LateralSpeed * LateralGrip);
		UpdatedPrimitive->AddTorque(AngularVelocity * -GroundAngularDamping);

		const float SpeedFactor = FMath::Clamp(std::abs(ForwardSpeed) / MaxSpeed, 0.35f, 1.0f);
		float Torque = SteeringInput * SteeringPower * SpeedFactor;
		if (ForwardSpeed < 0.0f)
		{
			Torque = -Torque;
		}
		UpdatedPrimitive->AddTorque(Up * Torque);
	}

	return GroundedWheelCount > 0;
}

void UCarMovementComponent::ApplyAirSteering(float DeltaTime)
{
	if (!UpdatedPrimitive) return;

	float ForwardSpeed = GetForwardSpeed();
	float SpeedFactor = FMath::Clamp(std::abs(ForwardSpeed) / MaxSpeed, 0.35f, 1.0f);

	float Torque = SteeringInput * SteeringPower * SpeedFactor;

	if (ForwardSpeed < 0.0f)
	{
		Torque = -Torque;
	}

	UpdatedPrimitive->AddTorque(UpdatedPrimitive->GetUpVector() * Torque * AirSteeringScale);
}

void UCarMovementComponent::ApplyRigidBodyMovement(float DeltaTime)
{
	if (!UpdatedPrimitive) return;

	FVector Forward = UpdatedPrimitive->GetForwardVector();
	FVector Right = UpdatedPrimitive->GetRightVector();
	FVector Velocity = UpdatedPrimitive->GetLinearVelocity();
	float ForwardSpeed = Forward.Dot(Velocity);

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

	const float SpeedFactor = FMath::Clamp(std::abs(ForwardSpeed) / MaxSpeed, 0.35f, 1.0f);
	float Torque = SteeringInput * SteeringPower * SpeedFactor;
	if (ForwardSpeed < 0.0f)
	{
		Torque = -Torque;
	}
	UpdatedPrimitive->AddTorque(UpdatedPrimitive->GetUpVector() * Torque);

	const float LateralSpeed = Velocity.Dot(Right);
	UpdatedPrimitive->AddForce(Right * -LateralSpeed * LateralGrip);
}

FVector UCarMovementComponent::GetPlaneNormal() const
{
	if (!UpdatedPrimitive) return FVector::UpVector;

	FVector Forward = UpdatedPrimitive->GetForwardVector();
	FVector Right = UpdatedPrimitive->GetRightVector();
	return Forward.Cross(Right).Normalized();
}

void UCarMovementComponent::CacheWheelComponents()
{
	WheelComponents.clear();

	for (USceneComponent* Component : GetOwnerSceneComponents())
	{
		if (USphereComponent* Sphere = Cast<USphereComponent>(Component))
		{
			WheelComponents.push_back(Sphere);
			if (bDisableWheelCollision)
			{
				Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}
