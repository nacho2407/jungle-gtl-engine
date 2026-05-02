// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "SpotLightActor.h"
#include "Component/SpotLightComponent.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ASpotLightActor, AActor)

ASpotLightActor::ASpotLightActor()
{
    SetActorTickEnabled(false);
    bTickInEditor = false;
}

void ASpotLightActor::InitDefaultComponents()
{
    SpotLightComponent = AddComponent<USpotLightComponent>();
    SetRootComponent(SpotLightComponent);

    BillboardComponent = AddComponent<UBillboardComponent>();
    auto SpotLightIcon = FMaterialManager::Get().GetOrCreateMaterial(SpotLightIconPath);
    BillboardComponent->SetVisibleInEditor(true);
    BillboardComponent->SetVisibleInGame(false);
    BillboardComponent->SetEditorHelper(true);
    BillboardComponent->SetMaterial(SpotLightIcon);
    BillboardComponent->AttachToComponent(SpotLightComponent);
}
