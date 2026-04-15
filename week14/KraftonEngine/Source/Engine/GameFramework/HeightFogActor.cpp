#include "HeightFogActor.h"
#include "Component/HeightFogComponent.h"
#include "Component/BillboardComponent.h"

IMPLEMENT_CLASS(AHeightFogActor, AActor)

AHeightFogActor::AHeightFogActor()
{
}

void AHeightFogActor::InitDefaultComponents()
{
	FogComponent = AddComponent<UHeightFogComponent>();
	SetRootComponent(FogComponent);

	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->AttachToComponent(FogComponent);
	BillboardComponent->SetTexture("HeightFog");
}
