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

	virtual void PostDuplicate(UObject* Original) override;

	// 프로퍼티 시스템 — UObject 에서 상속
	// UWorld 는 현재 에디터에 노출할 스칼라 프로퍼티가 없습니다.
	// (PersistentLevel 은 PostDuplicate 에서 별도 처리)
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override {}
	void PostEditProperty(const char* PropertyName) override {}

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
        SpatialIndex.FlushDirtyBounds();
        return Actor;
    }

    void DestroyActor(AActor* Actor) 
	{
        if (!Actor) return;

        Actor->EndPlay(EEndPlayReason::Type::Destroyed);
		PersistentLevel->RemoveActor(Actor);
        Actor->SetWorld(nullptr);
        UObjectManager::Get().DestroyObject(Actor);
    }



	TArray<AActor*> GetActors() const { return PersistentLevel->GetActors(); }

	ULevel* GetPersistentLevel() const { return PersistentLevel; }

    void BeginPlay();      // Triggers BeginPlay on all actors
    void Tick(float DeltaTime);  // Drives the game loop every frame
    void EndPlay(EEndPlayReason::Type EndPlayReason); // Cleanup before world is destroyed

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

	EWorldType GetWorldType() const { return WorldType; }
	void SetWorldType(EWorldType InWorldType) { WorldType = InWorldType; }

private:
	EWorldType WorldType = EWorldType::Editor;
	ULevel* PersistentLevel = nullptr;
	FViewportCamera* ActiveCamera = nullptr;
    FWorldSpatialIndex SpatialIndex;
    bool bHasBegunPlay = false;
};
