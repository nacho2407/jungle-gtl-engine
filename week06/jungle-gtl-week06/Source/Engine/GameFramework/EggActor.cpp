#include "EggActor.h"
#include "Component/ProjectileMovementComponent.h"

IMPLEMENT_CLASS(AEggActor, AStaticMeshActor)

void AEggActor::InitDefaultComponents()
{
	AStaticMeshActor::InitDefaultComponents("Data/Egg/egg.OBJ");
	ProjectileComponent = AddComponent<UProjectileMovementComponent>();
	ProjectileComponent->SetInitialSpeed(10.f);
	ProjectileComponent->SetVelocity(FVector(1.0f, 0, 1.0f));
}