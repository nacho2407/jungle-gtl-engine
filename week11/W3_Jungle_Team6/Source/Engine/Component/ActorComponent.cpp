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

void UActorComponent::ExecuteTick(float DeltaTime)
{
	if (bCanEverTick == false || bIsActive == false)
	{
		return;
	}

	TickComponent(DeltaTime);
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
