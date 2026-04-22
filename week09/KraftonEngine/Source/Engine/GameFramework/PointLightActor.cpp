#include "PointLightActor.h"
#include "Component/PointLightComponent.h"
#include "Component/CylindricalBillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(APointLightActor, AActor)

APointLightActor::APointLightActor()
{
	bNeedsTick = false;
	bTickInEditor = false;
}

void APointLightActor::InitDefaultComponents()
{
    PointLightComponent = AddComponent<UPointLightComponent>();
    SetRootComponent(PointLightComponent);
	
	BillboardComponent = AddComponent<UBillboardComponent>();
	auto PointLightIcon = FMaterialManager::Get().GetOrCreateMaterial(PointLightIconPath);
	BillboardComponent->SetVisibleInEditor(true);
	BillboardComponent->SetVisibleInGame(false);
	BillboardComponent->SetEditorHelper(true);
	BillboardComponent->SetMaterial(PointLightIcon);
	BillboardComponent->AttachToComponent(PointLightComponent);
}