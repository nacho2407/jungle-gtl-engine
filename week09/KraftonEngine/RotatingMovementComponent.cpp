#include "RotatingMovementComponent.h"

#include "Component/SceneComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(URotatingMovementComponent, UMovementComponent)

void URotatingMovementComponent::TickComponent(float DeltaTime)
{
	UMovementComponent::TickComponent(DeltaTime);

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

	const FRotator NewRotation = UpdatedSceneComponent->GetRelativeRotation() + (RotationRate * DeltaTime);
	UpdatedSceneComponent->SetRelativeRotation(NewRotation);
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Rotation Rate", EPropertyType::Rotator, &RotationRate, 0.0f, 0.0f, 0.1f });
}
