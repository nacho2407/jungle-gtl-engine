#pragma once
#include "Object/Object.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "GameFramework/AActor.h"
#include "Render/Proxy/FScene.h"

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

private:
	struct FPickingBVHLeaf
	{
		FBoundingBox Bounds;
		UPrimitiveComponent* Primitive = nullptr;
		AActor* Owner = nullptr;
	};

	struct FPickingBVHNode
	{
		FBoundingBox Bounds;
		int32 LeftChild = -1;
		int32 RightChild = -1;
		int32 FirstLeaf = 0;
		int32 LeafCount = 0;

		bool IsLeaf() const { return LeftChild < 0 && RightChild < 0; }
	};

	void EnsurePickingBVH() const;
	void RebuildPickingBVH() const;
	int32 BuildPickingBVHRecursive(int32 Start, int32 End) const;

	TArray<AActor*> Actors;
	UCameraComponent* ActiveCamera = nullptr;
	bool bHasBegunPlay = false;
	mutable bool bPickingBVHDirty = true;
	mutable TArray<FPickingBVHLeaf> PickingLeaves;
	mutable TArray<FPickingBVHNode> PickingNodes;
	FScene Scene;
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
