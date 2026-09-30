// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "AmbientLightActor.h"
#include "Component/AmbientLightComponent.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(AAmbientLightActor, AActor)

AAmbientLightActor::AAmbientLightActor()
{
    bNeedsTick = false;
    bTickInEditor = false;
}

void AAmbientLightActor::InitDefaultComponents()
{
    AmbientLightComponent = AddComponent<UAmbientLightComponent>();
    SetRootComponent(AmbientLightComponent);

    BillboardComponent = AddComponent<UBillboardComponent>();
    BillboardComponent->AttachToComponent(AmbientLightComponent);
    auto AmbientLightIcon = FMaterialManager::Get().GetOrCreateMaterial(AmbientLightIconPath);
    BillboardComponent->SetVisibleInEditor(true);
    BillboardComponent->SetVisibleInGame(false);
    BillboardComponent->SetEditorHelper(true);
    BillboardComponent->SetMaterial(AmbientLightIcon);
}
