#include "Actor/AActor.h"
#include "Component/UActorComponent.h"
#include "Component/USceneComponent.h"

AActor::AActor() : RootComponent(nullptr) { }
AActor::AActor(const FUObjectInitializer& ObjectInitializer) : UObject(ObjectInitializer) {}   // 추가
AActor::~AActor() { }

void AActor::BeginPlay()
{
	for (UActorComponent* Component : Components)
	{
		if (Component)
		{
			Component->BeginPlay();
		}
	}
}

void AActor::Tick(float DeltaTime)
{
	for (UActorComponent* Component : Components)
	{
		if (Component)
		{
			Component->TickComponent(DeltaTime);
		}
	}
}

void AActor::AddComponent(UActorComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	InComponent->SetOwner(this);
	Components.push_back(InComponent);
}

const TArray<UActorComponent*>& AActor::GetComponents() const
{
	return Components;
}

void AActor::SetRootComponent(USceneComponent* InRootComponent)
{
	RootComponent = InRootComponent;
}

USceneComponent* AActor::GetRootComponent() const
{
	return RootComponent;
}
