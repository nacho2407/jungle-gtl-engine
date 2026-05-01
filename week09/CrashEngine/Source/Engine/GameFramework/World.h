// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Object/Object.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Collision/BVH/EditorPickingBVH.h"
#include "Collision/BVH/ScenePrimitiveBVH.h"
#include "GameFramework/AActor.h"
#include "GameFramework/WorldContext.h"
#include "GameFramework/Level.h"
#include "Component/CameraComponent.h"
#include "Render/Scene/Scene.h"
#include "Render/Visibility/LOD/LODContext.h"
#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>

class UCameraComponent;
class UPrimitiveComponent;
class FCollisionManager;

    // UWorld는 월드 실행 상태와 레벨 구성을 관리합니다.
class UWorld : public UObject
{
public:
    DECLARE_CLASS(UWorld, UObject)
    UWorld();
    ~UWorld() override;

    // PIE 월드 복제용 — 자체 Actor 리스트를 순회하며 각 Actor를 새 World로 Duplicate.
    // UObject::Duplicate는 Serialize 왕복만 수행하므로 UWorld처럼 컨테이너 기반 상태가 있는
    // 타입은 별도 오버라이드가 필요하다.
    UObject* Duplicate(UObject* NewOuter = nullptr) const override;

    // Actor lifecycle
    template <typename T>
    T* SpawnActor();
    void DestroyActor(AActor* Actor);
    void AddActor(AActor* Actor);
    void MarkEditorPickingAndScenePrimitiveBVHsDirty();
    void BuildEditorPickingBVHNow() const;
    const FEditorPickingBVH& GetEditorPickingBVH() const { return EditorPickingBVH; }
    void BuildScenePrimitiveBVHNow() const;
    const FScenePrimitiveBVH& GetScenePrimitiveBVH() const { return ScenePrimitiveBVH; }
    void BeginDeferredPickingBVHUpdate();
    void EndDeferredPickingBVHUpdate();
    void WarmupPickingData() const;
    bool RaycastEditorPicking(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const;

    const TArray<AActor*>& GetActors() const { return PersistentLevel->GetActors(); }

    // LOD 컨텍스트를 FSceneView에 전달 (Collect 단계에서 LOD 인라인 갱신용)
    FLODUpdateContext PrepareLODContext();

    void InitWorld();                                // Set up the world before gameplay begins
    void BeginPlay();                                // Triggers BeginPlay on all actors
    void Tick(float DeltaTime, ELevelTick TickType); // Drives the game loop every frame
    void EndPlay();                                  // Cleanup before world is destroyed

    bool HasBegunPlay() const { return bHasBegunPlay; }

    // Active Camera — EditorViewportClient 또는 PlayerController가 세팅
    void SetActiveCamera(UCameraComponent* InCamera) { ActiveCamera = InCamera; }
    UCameraComponent* GetActiveCamera() const { return ActiveCamera; }

    // FScene — 렌더 프록시 관리자
    FScene& GetScene() { return Scene; }
    const FScene& GetScene() const { return Scene; }

    FSpatialPartition& GetPartition() { return Partition; }
    const FOctree* GetOctree() const { return Partition.GetOctree(); }
    void InsertActorToOctree(AActor* actor);
    void RemoveActorToOctree(AActor* actor);
    void SetWorldType(EWorldType InWorldType) { WorldType = InWorldType; }
    EWorldType GetWorldType() const { return WorldType; }
    void UpdateActorInOctree(AActor* actor);

	FCollisionManager* GetCollisionManager() { return CollisionManager.get(); }

private:
    // TArray<AActor*> Actors;
    ULevel* PersistentLevel;

    UCameraComponent* ActiveCamera = nullptr;
    UCameraComponent* LastLODUpdateCamera = nullptr;
    bool bHasBegunPlay = false;
    bool bHasLastFullLODUpdateCameraPos = false;
    mutable FEditorPickingBVH EditorPickingBVH;
    mutable FScenePrimitiveBVH ScenePrimitiveBVH;
    int32 DeferredPickingBVHUpdateDepth = 0;
    bool bDeferredPickingBVHDirty = false;
    uint32 VisibleProxyBuildFrame = 0;
    FVector LastFullLODUpdateCameraForward = FVector(1, 0, 0);
    FVector LastFullLODUpdateCameraPos = FVector(0, 0, 0);
    FScene Scene;
    FTickManager TickManager;

    FSpatialPartition Partition;
    EWorldType WorldType = EWorldType::Editor;

	std::unique_ptr<FCollisionManager> CollisionManager;
};

template <typename T>
inline T* UWorld::SpawnActor()
{
    // create and register an actor
    T* Actor = UObjectManager::Get().CreateObject<T>(PersistentLevel);
    AddActor(Actor); // BeginPlay 트리거는 AddActor 내부에서 bHasBegunPlay 가드로 처리
    return Actor;
}
