#include "DirectionalLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Materials/MaterialManager.h"
IMPLEMENT_CLASS(ADirectionalLightActor, AActor)

void ADirectionalLightActor::InitDefaultComponents()
{
	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetEditorOnly(true);
	SetRootComponent(BillboardComponent);

	auto LightMaterial = FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/DirectionalLight.json");
	BillboardComponent->SetMaterial(LightMaterial);

	LightComponent = AddComponent<UDirectionalLightComponent>();
	LightComponent->AttachToComponent(BillboardComponent);
}
