#include "GameFramework/CapsuleActor.h"
#include "Component/CapsuleComponent.h"

IMPLEMENT_CLASS(ACapsuleActor, AActor)

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
