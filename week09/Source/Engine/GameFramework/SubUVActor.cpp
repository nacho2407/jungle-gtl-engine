// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "GameFramework/SubUVActor.h"

#include "Component/SubUVComponent.h"

IMPLEMENT_CLASS(ASubUVActor, AActor)

ASubUVActor::ASubUVActor()
{
    bTickInEditor = true;
}

void ASubUVActor::InitDefaultComponents()
{
    SubUVComponent = AddComponent<USubUVComponent>();
    SetRootComponent(SubUVComponent);
    SubUVComponent->SetVisibility(true);
    SubUVComponent->SetVisibleInEditor(true);
    SubUVComponent->SetVisibleInGame(true);
    SubUVComponent->SetSpriteSize(10.0f, 10.0f);
}
