// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "GameFramework/CameraActor.h"

#include "Component/BillboardComponent.h"
#include "Component/CameraBoomComponent.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ACameraActor, AActor)

ACameraActor::ACameraActor()
{
    bTickInEditor = true;
}

void ACameraActor::InitDefaultComponents()
{
    CameraBoomComponent = AddComponent<UCameraBoomComponent>();
    SetRootComponent(CameraBoomComponent);

    CameraComponent = AddComponent<UCameraComponent>();
    CameraComponent->AttachToComponent(CameraBoomComponent);
    CameraComponent->SetMainCamera(true);

    BillboardComponent = AddComponent<UBillboardComponent>();
    BillboardComponent->AttachToComponent(CameraBoomComponent);
    BillboardComponent->SetVisibleInEditor(true);
    BillboardComponent->SetVisibleInGame(false);
    BillboardComponent->SetEditorHelper(true);

    auto CameraIcon = FMaterialManager::Get().GetOrCreateMaterial(CameraActorIconPath);
    BillboardComponent->SetMaterial(CameraIcon);
}

void ACameraActor::BeginPlay()
{
    AActor::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        if (CameraComponent && CameraComponent->IsMainCamera())
        {
            World->SetActiveCamera(CameraComponent);
        }
    }
}
