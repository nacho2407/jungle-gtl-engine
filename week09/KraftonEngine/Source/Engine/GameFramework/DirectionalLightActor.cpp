#include "DirectionalLightActor.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ADirectionalLightActor, AActor)

ADirectionalLightActor::ADirectionalLightActor()
{
	bNeedsTick = false;
	bTickInEditor = false;
}

void ADirectionalLightActor::InitDefaultComponents()
{
    DirectionalLightComponent = AddComponent<UDirectionalLightComponent>();
    SetRootComponent(DirectionalLightComponent);
	
	BillboardComponent = AddComponent<UBillboardComponent>();
	auto DirectionalLightIcon = FMaterialManager::Get().GetOrCreateMaterial(DirectionalLightIconPath);
	BillboardComponent->SetMaterial(DirectionalLightIcon);
	BillboardComponent->AttachToComponent(DirectionalLightComponent);
}