// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "HeightFogActor.h"
#include "Component/HeightFogComponent.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Platform/Paths.h"

IMPLEMENT_CLASS(AHeightFogActor, AActor)

AHeightFogActor::AHeightFogActor()
{
}

void AHeightFogActor::InitDefaultComponents()
{
    FogComponent = AddComponent<UHeightFogComponent>();
    SetRootComponent(FogComponent);

    BillboardComponent = AddComponent<UBillboardComponent>();
    BillboardComponent->AttachToComponent(FogComponent);

    auto FogMaterial = FMaterialManager::Get().GetOrCreateMaterial(FPaths::EditorRelativePath("Icons/Materials/HeightFog.json"));
    BillboardComponent->SetVisibleInEditor(true);
    BillboardComponent->SetVisibleInGame(false);
    BillboardComponent->SetEditorHelper(true);
    BillboardComponent->SetMaterial(FogMaterial);
}
