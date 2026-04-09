#include "RotatingMovementComponent.h"

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(URotatingMovementComponent, UMovementComponent)

void URotatingMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

	// AddLocalRotation은 내부에서 quat 합성으로 누적하므로 짐벌락에 안전.
	UpdatedSceneComponent->AddLocalRotation(RotationRate * DeltaTime);
}

void URotatingMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << RotationRate.Pitch;
	Ar << RotationRate.Yaw;
	Ar << RotationRate.Roll;
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Rotation Rate", EPropertyType::Rotator, &RotationRate, 0.0f, 0.0f, 0.1f });
}
