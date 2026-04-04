#pragma once
#include "Object/Object.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Collision/PickingBVH.h"
#include "GameFramework/AActor.h"
#include "Render/Proxy/FScene.h"
#include "Render/DebugDraw/DebugDrawQueue.h"
#include "Math/ConvexVolume.h"
#include <Collision/Octree.h>

class UCameraComponent;
class UPrimitiveComponent;

class UWorld : public UObject {
public:
	DECLARE_CLASS(UWorld, UObject)
	UWorld() = default;
	~UWorld() override;

	// Actor lifecycle
	template<typename T>
	T* SpawnActor();
	void DestroyActor(AActor* Actor);
	void AddActor(AActor* Actor);
	void MarkPickingBVHDirty();
	void BuildPickingBVHNow() const;
	bool RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const;

	const TArray<AActor*>& GetActors() const { return Actors; }
	const TArray<FPrimitiveSceneProxy*>& GetVisibleProxies() const { return VisibleProxies; }
	void UpdateVisibleProxies();

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

	FOctree* GetOctree() { return Octree; }
	void InsertActorToOctree(AActor* actor);
	void RemoveActorToOctree(AActor* actor);
	void UpdateActorInOctree(AActor* actor);
private:
	TArray<AActor*> Actors;
	TArray<UPrimitiveComponent*> VisiblePrimitives;
	TArray<FPrimitiveSceneProxy*> VisibleProxies;

	UCameraComponent* ActiveCamera = nullptr;
	bool bHasBegunPlay = false;
	mutable FPickingBVH PickingBVH;
	FScene Scene;
	FDebugDrawQueue DebugDrawQueue;

	FOctree* Octree;
};

template<typename T>
inline T* UWorld::SpawnActor()
{
	// create and register an actor
	T* Actor = UObjectManager::Get().CreateObject<T>();
	Actor->SetWorld(this);
	if (bHasBegunPlay)
	{
		Actor->BeginPlay();
	}
	AddActor(Actor);
	return Actor;
}
