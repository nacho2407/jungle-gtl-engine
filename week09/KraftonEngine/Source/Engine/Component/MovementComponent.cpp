#include "Component/MovementComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UMovementComponent, UActorComponent)

void UMovementComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	TryAutoRegisterUpdatedComponent();
}

void UMovementComponent::TickComponent(float DeltaTime)
{
	//일단 여타 컴포넌트들과 마찬가지로 처리. 비어 있음.
	UActorComponent::TickComponent(DeltaTime);
}

void UMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Auto Register Updated", EPropertyType::Bool, &bAutoRegisterUpdatedComponent });
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	UpdatedComponent = NewUpdatedComponent;
}

void UMovementComponent::TryAutoRegisterUpdatedComponent()
{
	if (!bAutoRegisterUpdatedComponent || UpdatedComponent != nullptr)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	SetUpdatedComponent(OwnerActor->GetRootComponent());
}
