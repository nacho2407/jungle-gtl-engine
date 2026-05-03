#include "FloatingPawnMovementComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>

IMPLEMENT_CLASS(UFloatingPawnMovementComponent, UMovementComponent)

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
	const float ClampedRotationInput = std::clamp(RotationInput, -1.0f, 1.0f);

	UpdatedPrimitive->SetLinearVelocity(UpdatedPrimitive->GetForwardVector() * (ClampedMoveInput * Speed));
	UpdatedPrimitive->SetAngularVelocity(UpdatedPrimitive->GetUpVector() * (ClampedRotationInput * RotationSpeed * FMath::DegToRad));
}

void UFloatingPawnMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Speed", EPropertyType::Float, "Movement", &Speed, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "RotationSpeed", EPropertyType::Float, "Movement", &RotationSpeed, 0.0f, 100.0f, 0.1f });
}

void UFloatingPawnMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << Speed;
	Ar << RotationSpeed;
}
