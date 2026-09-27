#include "ActorComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UActorComponent, UObject)

UActorComponent::UActorComponent()
{
	PrimaryComponentTick.Target = this;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;	
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bTickInEditor = false;
	PrimaryComponentTick.RegisterTickFunction();
}

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
	SetComponentTickEnabled(true);
}

void UActorComponent::Deactivate()
{
	bIsActive = false;
	SetComponentTickEnabled(false);
}


void UActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
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

void UActorComponent::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	Ar << PrimaryComponentTick.bTickEnabled;
	Ar << PrimaryComponentTick.bTickInEditor;
}

void UActorComponent::SetOwner(AActor* Actor)
{
	Owner = Actor;
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	//OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
	//OutProps.push_back({ "Auto Activate", EPropertyType::Bool, &bAutoActivate });
	//OutProps.push_back({ "Can Ever Tick", EPropertyType::Bool, &bCanEverTick });
	OutProps.push_back({ "bTickEnable", EPropertyType::Bool, &PrimaryComponentTick.bTickEnabled });
	OutProps.push_back({ "bTickInEditor", EPropertyType::Bool, &PrimaryComponentTick.bTickInEditor });
}

void UActorComponent::PostEditProperty(const char* PropertyName)
{
}
