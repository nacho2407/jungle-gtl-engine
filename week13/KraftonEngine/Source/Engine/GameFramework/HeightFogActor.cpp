#include "HeightFogActor.h"
#include "Component/Primitive/HeightFogComponent.h"
#include "Component/Primitive/BillboardComponent.h"
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
