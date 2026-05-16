#include "Game/Component/Movement/CarMovementComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SphereComponent.h"
#include "Core/CollisionTypes.h"
#include "Serialization/Archive.h"
#include "Core/Log.h"

#include <algorithm>

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

float UCarMovementComponent::GetForwardSpeed() const
{
	if (!UpdatedPrimitive) return 0.0f;
	FVector Forward = UpdatedPrimitive->GetForwardVector();
	FVector Velocity = UpdatedPrimitive->GetLinearVelocity();

	return Forward.Dot(Velocity);
}

void UCarMovementComponent::StopImmediately()
{
	ThrottleInput = 0.0f;
	SteeringInput = 0.0f;

	if (!UpdatedPrimitive)
	{
		UpdatedPrimitive = Cast<UPrimitiveComponent>(GetUpdatedComponent());
	}

	if (!UpdatedPrimitive)
	{
		return;
	}

	UpdatedPrimitive->SetLinearVelocity(FVector::ZeroVector);
	UpdatedPrimitive->SetAngularVelocity(FVector::ZeroVector);
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
		// 지면 검출 — WorldStatic 채널에 Block 응답인 shape만 hit. trigger volume처럼
		// 응답이 모두 Overlap인 액터는 자동 제외되어 wheel suspension 판정이 흔들리지 않는다.
		if (!World->PhysicsRaycast(WheelRoot, Down, RayLength, Hit, ECollisionChannel::WorldStatic, OwnerActor))
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
