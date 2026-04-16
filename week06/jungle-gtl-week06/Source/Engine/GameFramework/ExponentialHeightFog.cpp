#include "ExponentialHeightFog.h"

#include "Component/ExponentialHeightFogComponent.h"

IMPLEMENT_CLASS(AExponentialHeightFog, AActor)

void AExponentialHeightFog::InitDefaultComponents()
{
	ExponentialHeightFogComponent = AddComponent<UExponentialHeightFogComponent>();
	SetRootComponent(ExponentialHeightFogComponent);
	ExponentialHeightFogComponent->SetWorldLocation(PendingActorLocation);
}
