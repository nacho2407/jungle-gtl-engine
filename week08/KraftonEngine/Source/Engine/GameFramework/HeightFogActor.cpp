#include "HeightFogActor.h"
#include "Component/HeightFogComponent.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"

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
	
	auto FogMaterial = FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/HeightFog.json");
	BillboardComponent->SetMaterial(FogMaterial);
}
