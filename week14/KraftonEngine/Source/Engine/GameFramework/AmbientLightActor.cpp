#include "AmbientLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/AmbientLightComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(AAmbientLightActor, AActor)

void AAmbientLightActor::InitDefaultComponents()
{
	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetEditorOnly(true);
	SetRootComponent(BillboardComponent);

	auto LightMaterial = FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/AmbientLight.json");
	BillboardComponent->SetMaterial(LightMaterial);

	LightComponent = AddComponent<UAmbientLightComponent>();
	LightComponent->AttachToComponent(BillboardComponent);
}
