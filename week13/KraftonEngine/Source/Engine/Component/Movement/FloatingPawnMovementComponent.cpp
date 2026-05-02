#include "FloatingPawnMovementComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Math/Rotator.h"
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

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

	UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedSceneComponent);

	const float ClampedMoveInput = std::clamp(MoveInput, -1.0f, 1.0f);
	const float ClampedRotationInput = std::clamp(RotationInput, -1.0f, 1.0f);
	const float MoveDistance = ClampedMoveInput * Speed * DeltaTime;
	const float YawDelta = ClampedRotationInput * RotationSpeed * DeltaTime;

	if (MoveDistance != 0.0f)
	{
		UpdatedSceneComponent->SetWorldLocation(
			UpdatedSceneComponent->GetWorldLocation() + UpdatedSceneComponent->GetForwardVector() * MoveDistance);
	}

	if (YawDelta != 0.0f)
	{
		UpdatedSceneComponent->AddLocalRotation(FRotator(0.0f, YawDelta, 0.0f));
	}
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
