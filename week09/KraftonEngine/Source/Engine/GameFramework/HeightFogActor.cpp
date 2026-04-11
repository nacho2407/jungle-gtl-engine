#include "HeightFogActor.h"
#include "Component/HeightFogComponent.h"

IMPLEMENT_CLASS(AHeightFogActor, AActor)

AHeightFogActor::AHeightFogActor()
{
}

void AHeightFogActor::InitDefaultComponents()
{
	FogComponent = AddComponent<UHeightFogComponent>();
	SetRootComponent(FogComponent);
}
