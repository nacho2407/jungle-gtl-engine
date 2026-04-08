#pragma once
#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Level.h"

class UCameraComponent;
class FViewportCamera;

class UWorld : public UObject {
public:
    DECLARE_CLASS(UWorld, UObject)
	UWorld();
	~UWorld() override;

	virtual UWorld* Duplicate() override;
	virtual UWorld* DuplicateSubObjects() override { return this; }

    // Actor lifecycle
    template<typename T>
    T* SpawnActor() 
	{
        // create and register an actor
        T* Actor = UObjectManager::Get().CreateObject<T>();
        Actor->SetWorld(this);
        if (bHasBegunPlay)
        {
            Actor->BeginPlay();
        }
		PersistentLevel->AddActor(Actor);
        return Actor;
    }
    void DestroyActor(AActor* Actor) 
	{
        // remove and clean up
        if (!Actor) return;
        Actor->EndPlay();
		PersistentLevel->RemoveActor(Actor);
        UObjectManager::Get().DestroyObject(Actor);
    }

	TArray<AActor*> GetActors() const { return PersistentLevel->GetActors(); }

	ULevel* GetPersistentLevel() const { return PersistentLevel; }

    void BeginPlay();      // Triggers BeginPlay on all actors
    void Tick(float DeltaTime);  // Drives the game loop every frame
    void EndPlay();        // Cleanup before world is destroyed

    bool HasBegunPlay() const { return bHasBegunPlay; }

    // Active Camera — EditorViewportClient 또는 PlayerController가 세팅
    void SetActiveCamera(FViewportCamera* InCamera) { ActiveCamera = InCamera; }
	FViewportCamera* GetActiveCamera() const { return ActiveCamera; }

private:
	ULevel* PersistentLevel = nullptr;
	FViewportCamera* ActiveCamera = nullptr;
    bool bHasBegunPlay = false;
};