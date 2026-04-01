#include "ActorComponent.h"

DEFINE_CLASS(UActorComponent, UObject)

void UActorComponent::BeginPlay()
{
	if (bAutoActivate)
	{
		Activate();
	}
}

void UActorComponent::Activate()
{
	bCanEverTick = true;
}

void UActorComponent::Deactivate()
{
	bCanEverTick = false;
}

void UActorComponent::TickComponent(float DeltaTime)
{
}

void UActorComponent::SetActive(bool bNewActive)
{
	if (bNewActive == bIsActive)
	{
		return;
	}

	bIsActive = bNewActive;

	if (bIsActive)
	{
		Activate();
	}
	else
	{
		Deactivate();
	}
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	//OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
	//OutProps.push_back({ "Auto Activate", EPropertyType::Bool, &bAutoActivate });
	//OutProps.push_back({ "Can Ever Tick", EPropertyType::Bool, &bCanEverTick });
}

void UActorComponent::Tick(float DeltaTime)
{
	if (bCanEverTick == false || bIsActive == false)
	{
		return;
	}

	TickComponent(DeltaTime);
}