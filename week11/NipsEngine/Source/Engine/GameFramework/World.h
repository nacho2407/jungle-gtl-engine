#pragma once
#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Level.h"
#include "Spatial/WorldSpatialIndex.h"

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
        SpatialIndex.RegisterActor(Actor);
        SpatialIndex.FlushDirtyBounds();
        return Actor;
    }
    void DestroyActor(AActor* Actor) 
	{
        // remove and clean up
        if (!Actor) return;
        SpatialIndex.UnregisterActor(Actor);
        Actor->EndPlay();
		PersistentLevel->RemoveActor(Actor);
        UObjectManager::Get().DestroyObject(Actor);
    }

	TArray<AActor*> GetActors() const { return PersistentLevel->GetActors(); }

	ULevel* GetPersistentLevel() const { return PersistentLevel; }

    void BeginPlay();      // Triggers BeginPlay on all actors
    void Tick(float DeltaTime);  // Drives the game loop every frame
    void EndPlay();        // Cleanup before world is destroyed

    /** @brief Rebuild the world BVH and bounds snapshot from all current primitives. */
    void RebuildSpatialIndex();

    /** @brief Flush pending bounds and visibility dirties into the world BVH. */
    void SyncSpatialIndex();

    bool HasBegunPlay() const { return bHasBegunPlay; }

    // Active Camera — EditorViewportClient 또는 PlayerController가 세팅
    void SetActiveCamera(FViewportCamera* InCamera) { ActiveCamera = InCamera; }
	FViewportCamera* GetActiveCamera() const { return ActiveCamera; }

    /** @brief Access the world-level primitive AABB/BVH manager. */
    FWorldSpatialIndex& GetSpatialIndex() { return SpatialIndex; }

    /** @brief Access the world-level primitive AABB/BVH manager. */
    const FWorldSpatialIndex& GetSpatialIndex() const { return SpatialIndex; }

private:
	ULevel* PersistentLevel = nullptr;
	FViewportCamera* ActiveCamera = nullptr;
    FWorldSpatialIndex SpatialIndex;
    bool bHasBegunPlay = false;
};
