#include "GameFramework/World.h"
#include "Component/CameraComponent.h"

DEFINE_CLASS(UWorld, UObject)
REGISTER_FACTORY(UWorld)

UWorld::~UWorld() {
    if (!Actors.empty()) {
        EndPlay();
    }
}

void UWorld::InitWorld() {
}

void UWorld::SetActiveCamera(UCameraComponent* Cam)
{
    ActiveCamera = Cam;
}

void UWorld::BeginPlay() {
    for (AActor* Actor : Actors) {
        if (Actor) {
            Actor->BeginPlay();
        }
    }
}

void UWorld::Tick(float DeltaTime) {
    for (AActor* Actor : Actors) {
        if (Actor) {
            Actor->Tick(DeltaTime);
        }
    }
}

void UWorld::EndPlay() {
    for (AActor* Actor : Actors) {
        if (Actor) {
            Actor->EndPlay();
            UObjectManager::Get().DestroyObject(Actor);
        }
    }

    Actors.clear();

    ActiveCamera = nullptr;
}
