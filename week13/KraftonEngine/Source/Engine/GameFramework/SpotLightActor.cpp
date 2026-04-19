#include "SpotLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/SpotLightComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(ASpotLightActor, AActor)

void ASpotLightActor::InitDefaultComponents()
{
	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetEditorOnly(true);
	SetRootComponent(BillboardComponent);

	auto LightMaterial = FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/SpotLight.json");
	BillboardComponent->SetMaterial(LightMaterial);

	LightComponent = AddComponent<USpotLightComponent>();
	LightComponent->AttachToComponent(BillboardComponent);
}
