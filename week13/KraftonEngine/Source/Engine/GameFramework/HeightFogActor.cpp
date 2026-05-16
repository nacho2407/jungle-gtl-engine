#include "HeightFogActor.h"
#include "Component/HeightFogComponent.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"

AHeightFogActor::AHeightFogActor()
{
}

void AHeightFogActor::InitDefaultComponents()
{
	FogComponent = AddComponent<UHeightFogComponent>();
	SetRootComponent(FogComponent);

	BillboardComponent = FogComponent->EnsureEditorBillboard();
}
