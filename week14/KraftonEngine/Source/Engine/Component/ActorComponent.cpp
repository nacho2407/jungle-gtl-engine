#include "ActorComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UActorComponent, UObject)

void UActorComponent::BeginPlay()
{
	if (bAutoActivate)
	{
		Activate();
	}
}

void UActorComponent::Activate()
{
	bIsActive = true;
	PrimaryComponentTick.SetTickEnabled(true);
}

void UActorComponent::Deactivate()
{
	bIsActive = false;
	PrimaryComponentTick.SetTickEnabled(false);
}


void UActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	if (GetOwner() == nullptr)
	{
		return;
	}

	if (PrimaryComponentTick.bTickEnabled == false)
	{
		return;
	}

	if (bIsActive == false)
	{
		return;
	}

	Tick(DeltaTime);
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

void UActorComponent::SetOwner(AActor* Actor)
{
	Owner = Actor;
	PrimaryComponentTick.Target = this;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	//OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
	//OutProps.push_back({ "Auto Activate", EPropertyType::Bool, &bAutoActivate });
	//OutProps.push_back({ "Can Ever Tick", EPropertyType::Bool, &bCanEverTick });
	OutProps.push_back({ "bTickEnable", EPropertyType::Bool, &PrimaryComponentTick.bTickEnabled });
}

void UActorComponent::Tick(float DeltaTime)
{
	
}