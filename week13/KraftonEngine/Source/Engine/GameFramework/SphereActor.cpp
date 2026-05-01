#include "GameFramework/SphereActor.h"
#include "Component/SphereComponent.h"
#include "Core/Log.h"

IMPLEMENT_CLASS(ASphereActor, AActor)

void ASphereActor::InitDefaultComponents()
{
	SphereComponent = AddComponent<USphereComponent>();
	SetRootComponent(SphereComponent);
	SphereComponent->SetSphereRadius(1.0f);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SphereComponent->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
}

void ASphereActor::PostDuplicate()
{
	SphereComponent = Cast<USphereComponent>(GetRootComponent());
}

void ASphereActor::BeginPlay()
{
	Super::BeginPlay();
}
