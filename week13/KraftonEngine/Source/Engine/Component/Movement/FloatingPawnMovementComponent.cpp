#include "FloatingPawnMovementComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UFloatingPawnMovementComponent, UMovementComponent)

namespace
{
	void AddWorldRotation(USceneComponent* Component, const FQuat& DeltaWorldQuat)
	{
		if (!Component)
		{
			return;
		}

		const FQuat CurrentWorldQuat = FQuat::FromMatrix(Component->GetWorldMatrix());
		const FQuat NewWorldQuat = (DeltaWorldQuat * CurrentWorldQuat).GetNormalized();

		if (USceneComponent* Parent = Component->GetParent())
		{
			const FQuat ParentWorldQuat = FQuat::FromMatrix(Parent->GetWorldMatrix());
			Component->SetRelativeRotation((NewWorldQuat * ParentWorldQuat.Inverse()).GetNormalized());
			return;
		}

		Component->SetRelativeRotation(NewWorldQuat);
	}
}

void UFloatingPawnMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();
	UpdatedPrimitive = Cast<UPrimitiveComponent>(GetUpdatedComponent());
}

void UFloatingPawnMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatedPrimitive = Cast<UPrimitiveComponent>(GetUpdatedComponent());
	if (!UpdatedPrimitive)
	{
		return;
	}

	const float ClampedMoveInput = std::clamp(MoveInput, -1.0f, 1.0f);
	const float ClampedRightMoveInput = std::clamp(RightMoveInput, -1.0f, 1.0f);
	const float MouseYawDelta = LookInputX * MouseSensitivity;
	const float MousePitchDelta = LookInputY * MouseSensitivity;

	USceneComponent* RotationComponent = UpdatedPrimitive;
	const TArray<USceneComponent*>& Children = UpdatedPrimitive->GetChildren();
	if (!Children.empty() && Children[0])
	{
		RotationComponent = Children[0];
	}

	FVector Forward = RotationComponent->GetForwardVector();
	Forward.Z = 0.0f;
	Forward.Normalize();

	FVector Right = RotationComponent->GetRightVector();
	Right.Z = 0.0f;
	Right.Normalize();

	FVector MoveDirection = Forward * ClampedMoveInput + Right * ClampedRightMoveInput;
	if (MoveDirection.Length() > 1.0f)
	{
		MoveDirection.Normalize();
	}

	UpdatedPrimitive->SetLinearVelocity(MoveDirection * Speed);

	if (MouseYawDelta != 0.0f || MousePitchDelta != 0.0f)
	{
		const FQuat YawWorldQuat = FQuat::FromAxisAngle(FVector::UpVector, MouseYawDelta * DEG_TO_RAD);
		const FVector PitchAxisWorld = YawWorldQuat.RotateVector(RotationComponent->GetRightVector()).Normalized();
		const FQuat PitchWorldQuat = FQuat::FromAxisAngle(PitchAxisWorld, MousePitchDelta * DEG_TO_RAD);
		AddWorldRotation(RotationComponent, PitchWorldQuat * YawWorldQuat);
	}

	LookInputX = 0.0f;
	LookInputY = 0.0f;
}

void UFloatingPawnMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Speed", EPropertyType::Float, "Movement", &Speed, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "MouseSensitivity", EPropertyType::Float, "Movement", &MouseSensitivity, 0.0f, 10.0f, 0.01f });
}

void UFloatingPawnMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << Speed;
	Ar << MouseSensitivity;
}
