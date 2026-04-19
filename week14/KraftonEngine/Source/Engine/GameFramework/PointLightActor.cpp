#include "PointLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/PointLightComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(APointLightActor, AActor)

void APointLightActor::InitDefaultComponents()
{
	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetEditorOnly(true);
	SetRootComponent(BillboardComponent);

	auto LightMaterial = FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/PointLight.json");
	BillboardComponent->SetMaterial(LightMaterial);

	LightComponent = AddComponent<UPointLightComponent>();
	LightComponent->AttachToComponent(BillboardComponent);
}
