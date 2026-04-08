#pragma once
#include "Object/Object.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Collision/WorldPrimitivePickingBVH.h"
#include "GameFramework/AActor.h"
#include "Component/CameraComponent.h"
#include "Render/Proxy/FScene.h"
#include "Render/DebugDraw/DebugDrawQueue.h"
#include "Render/Culling/ConvexVolume.h"
#include "Render/Pipeline/LODContext.h"
#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>

class UCameraComponent;
class UPrimitiveComponent;

class UWorld : public UObject {
public:
	DECLARE_CLASS(UWorld, UObject)
	UWorld() = default;
	~UWorld() override;

	// PIE 월드 복제용 — 자체 Actor 리스트를 순회하며 각 Actor를 새 World로 Duplicate.
	// UObject::Duplicate는 Serialize 왕복만 수행하므로 UWorld처럼 컨테이너 기반 상태가 있는
	// 타입은 별도 오버라이드가 필요하다.
	UObject* Duplicate(UObject* NewOuter = nullptr) const override;

	// Actor lifecycle
	template<typename T>
	T* SpawnActor();
	void DestroyActor(AActor* Actor);
	void AddActor(AActor* Actor);
	void MarkWorldPrimitivePickingBVHDirty();
	void BuildWorldPrimitivePickingBVHNow() const;
	void BeginDeferredPickingBVHUpdate();
	void EndDeferredPickingBVHUpdate();
	void WarmupPickingData() const;
	bool RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const;
	void InvalidateVisibleSet();

	const TArray<AActor*>& GetActors() const { return Actors; }
	const TArray<FPrimitiveSceneProxy*>& GetVisibleProxies() const { return VisibleProxies; }
	void UpdateVisibleProxies();

	// LOD 컨텍스트를 RenderBus에 전달 (Collect 단계에서 LOD 인라인 갱신용)
	FLODUpdateContext PrepareLODContext();

	void InitWorld();      // Set up the world before gameplay begins
	void BeginPlay();      // Triggers BeginPlay on all actors
	void Tick(float DeltaTime);  // Drives the game loop every frame
	void EndPlay();        // Cleanup before world is destroyed

	bool HasBegunPlay() const { return bHasBegunPlay; }

	// Active Camera — EditorViewportClient 또는 PlayerController가 세팅
	void SetActiveCamera(UCameraComponent* InCamera) { ActiveCamera = InCamera; }
	UCameraComponent* GetActiveCamera() const { return ActiveCamera; }

	// FScene — 렌더 프록시 관리자
	FScene& GetScene() { return Scene; }
	const FScene& GetScene() const { return Scene; }
	
	// DebugDraw — DrawDebugLine 등 글로벌 함수가 사용
	FDebugDrawQueue& GetDebugDrawQueue() { return DebugDrawQueue; }
	const FDebugDrawQueue& GetDebugDrawQueue() const { return DebugDrawQueue; }

	const FOctree* GetOctree() const { return Partition.GetOctree(); }
	void InsertActorToOctree(AActor* actor);
	void RemoveActorToOctree(AActor* actor);
	void UpdateActorInOctree(AActor* actor);

private:
	bool NeedsVisibleProxyRebuild() const;
	void CacheVisibleCameraState();

	TArray<AActor*> Actors;
	TArray<FPrimitiveSceneProxy*> VisibleProxies;

	UCameraComponent* ActiveCamera = nullptr;
	UCameraComponent* LastLODUpdateCamera = nullptr;
	bool bHasBegunPlay = false;
	bool bHasLastFullLODUpdateCameraPos = false;
	mutable FWorldPrimitivePickingBVH WorldPrimitivePickingBVH;
	int32 DeferredPickingBVHUpdateDepth = 0;
	bool bDeferredPickingBVHDirty = false;
	bool bVisibleSetDirty = true;
	bool bHasVisibleCameraState = false;
	uint32 VisibleProxyBuildFrame = 0;
	FVector LastVisibleCameraPos = FVector(0, 0, 0);
	FVector LastVisibleCameraForward = FVector(1, 0, 0);
	FCameraState LastVisibleCameraState = {};
	FVector LastFullLODUpdateCameraForward = FVector(1, 0, 0);
	FVector LastFullLODUpdateCameraPos = FVector(0, 0, 0);
	FScene Scene;
	FDebugDrawQueue DebugDrawQueue;

	FSpatialPartition Partition;
};

template<typename T>
inline T* UWorld::SpawnActor()
{
	// create and register an actor
	T* Actor = UObjectManager::Get().CreateObject<T>(this);
	AddActor(Actor); // BeginPlay 트리거는 AddActor 내부에서 bHasBegunPlay 가드로 처리
	return Actor;
}
