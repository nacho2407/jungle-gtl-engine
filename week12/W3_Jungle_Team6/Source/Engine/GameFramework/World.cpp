#include "GameFramework/World.h"

DEFINE_CLASS(UWorld, UObject)
REGISTER_FACTORY(UWorld)

UWorld::~UWorld() {
    if (!Actors.empty()) {
        EndPlay();
    }
}

void UWorld::InitWorld() {
}

void UWorld::SetActiveCamera(UCamera* Cam)
{
    if (ActiveCamera == Cam)
    {
        return;
    }

    if (ActiveCamera)
    {
        UObjectManager::Get().DestroyObject(ActiveCamera);
    }

    ActiveCamera = Cam;
}

void UWorld::BeginPlay() {
    for (AActor* Actor : Actors) {
        if (Actor && !Actor->bPendingKill) {
            Actor->BeginPlay();
        }
    }
}

void UWorld::Tick(float DeltaTime) {
    for (AActor* Actor : Actors) {
        if (Actor && !Actor->bPendingKill) {
            Actor->Tick(DeltaTime);
        }
    }

    // Cleanup destroyed objects here
}

void UWorld::EndPlay() {
    for (AActor* Actor : Actors) {
        if (Actor && !Actor->bPendingKill) {
            Actor->EndPlay();
            UObjectManager::Get().DestroyObject(Actor);
        }
    }

    Actors.clear();

    if (ActiveCamera) {
        UObjectManager::Get().DestroyObject(ActiveCamera);
    }

    ActiveCamera = nullptr;
    bPendingKill = true;
}