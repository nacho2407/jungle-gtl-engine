#include "GameFramework/Actor/BoxActor.h"
#include "Component/Shape/BoxComponent.h"
#include "Physics/BodyInstance.h"

void ABoxActor::InitDefaultComponents()
{
	BoxComponent = AddComponent<UBoxComponent>();
	SetRootComponent(BoxComponent);
	BoxComponent->SetBoxExtent(FVector(1.0f, 1.0f, 1.0f));
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoxComponent->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
}

void ABoxActor::PostDuplicate()
{
	BoxComponent = Cast<UBoxComponent>(GetRootComponent());
}
