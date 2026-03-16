#include "Component/UActorComponent.h"
#include "Actor/AActor.h"

UActorComponent::UActorComponent() : Owner(nullptr) {}
UActorComponent::~UActorComponent() {}

void UActorComponent::BeginPlay() {}
void UActorComponent::TickComponent(float DeltaTime) {}
void UActorComponent::SetOwner(AActor* InOwner)
{
	Owner = InOwner;
}
AActor* UActorComponent::GetOwner() const
{
	return Owner;
}