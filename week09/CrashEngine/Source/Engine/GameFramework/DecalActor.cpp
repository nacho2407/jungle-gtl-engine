// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "DecalActor.h"
#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(ADecalActor, AActor)

ADecalActor::ADecalActor()
    : DecalComponent(nullptr)
{
    SetActorTickEnabled(true);
    bTickInEditor = true;
}

void ADecalActor::InitDefaultComponents()
{
    DecalComponent = AddComponent<UDecalComponent>();
    auto Material = FMaterialManager::Get().GetOrCreateMaterial(DefaultDecalMaterialPath);
    DecalComponent->SetMaterial(0, Material);
    SetRootComponent(DecalComponent);

    BillboardComponent = AddComponent<UBillboardComponent>();
    auto DecalIcon = FMaterialManager::Get().GetOrCreateMaterial(DecalIconPath);
    BillboardComponent->SetVisibleInEditor(true);
    BillboardComponent->SetVisibleInGame(false);
    BillboardComponent->SetEditorHelper(true);
    BillboardComponent->SetMaterial(DecalIcon);
    BillboardComponent->AttachToComponent(DecalComponent);
}
