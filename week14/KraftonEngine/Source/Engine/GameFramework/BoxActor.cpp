#include "GameFramework/BoxActor.h"
#include "Component/BoxComponent.h"

IMPLEMENT_CLASS(ABoxActor, AActor)

void ABoxActor::InitDefaultComponents()
{
	BoxComponent = AddComponent<UBoxComponent>();
	SetRootComponent(BoxComponent);
	BoxComponent->SetBoxExtent(FVector(0.5f, 0.5f, 0.5f));
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoxComponent->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
}

void ABoxActor::PostDuplicate()
{
	BoxComponent = Cast<UBoxComponent>(GetRootComponent());
}
