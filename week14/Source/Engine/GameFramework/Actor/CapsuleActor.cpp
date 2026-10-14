#include "GameFramework/Actor/CapsuleActor.h"
#include "Component/Shape/CapsuleComponent.h"

void ACapsuleActor::InitDefaultComponents()
{
	CapsuleComponent = AddComponent<UCapsuleComponent>();
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->SetCapsuleSize(1.8f, 3.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleComponent->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
}

void ACapsuleActor::PostDuplicate()
{
	CapsuleComponent = Cast<UCapsuleComponent>(GetRootComponent());
}
