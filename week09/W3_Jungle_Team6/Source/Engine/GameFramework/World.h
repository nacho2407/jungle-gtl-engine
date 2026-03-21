#pragma once
#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Component/Camera.h"

#include "Engine/Core/InputSystem.h"

class UWorld : public UObject {
public:
    DECLARE_CLASS(UWorld, UObject)
    UWorld() = default;
    ~UWorld() override;

    // Actor lifecycle
    template<typename T>
    T* SpawnActor() {
        // create and register an actor
        T* Actor = UObjectManager::Get().CreateObject<T>();
        Actor->SetWorld(this);
        Actor->BeginPlay();
        Actors.push_back(Actor);
        return Actor;
    }
    void DestroyActor(AActor* Actor) {
        // remove and clean up
        if (!Actor) return;
        Actor->EndPlay();
        // Remove from actor list
        auto it = std::find(Actors.begin(), Actors.end(), Actor);
        if (it != Actors.end())
            Actors.erase(it);

        // Mark for garbage collection
        UObjectManager::Get().DestroyObject(Actor);
    }

    const TArray<AActor*>& GetActors() const { return Actors; }
    void AddActor(AActor* Actor) { Actors.push_back(Actor); }

    void InitWorld();      // Set up the world before gameplay begins
    void BeginPlay();      // Triggers BeginPlay on all actors
    void Tick(float DeltaTime);  // Drives the game loop every frame
    void EndPlay();        // Cleanup before world is destroyed

    void SetActiveCamera(UCameraComponent* Cam);
    UCameraComponent* GetActiveCamera() const { return ActiveCamera;  }

private:
    TArray<AActor*> Actors;
    UCameraComponent* ActiveCamera = nullptr;

    void CameraControls(float DeltaTime);
};