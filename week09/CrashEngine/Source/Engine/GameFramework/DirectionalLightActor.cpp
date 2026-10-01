// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "DirectionalLightActor.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ADirectionalLightActor, AActor)

ADirectionalLightActor::ADirectionalLightActor()
{
    SetActorTickEnabled(false);
    bTickInEditor = false;
}

void ADirectionalLightActor::InitDefaultComponents()
{
    DirectionalLightComponent = AddComponent<UDirectionalLightComponent>();
    SetRootComponent(DirectionalLightComponent);

    BillboardComponent = AddComponent<UBillboardComponent>();
    auto DirectionalLightIcon = FMaterialManager::Get().GetOrCreateMaterial(DirectionalLightIconPath);
    BillboardComponent->SetVisibleInEditor(true);
    BillboardComponent->SetVisibleInGame(false);
    BillboardComponent->SetEditorHelper(true);
    BillboardComponent->SetMaterial(DirectionalLightIcon);
    BillboardComponent->AttachToComponent(DirectionalLightComponent);
}
