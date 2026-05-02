#pragma once
#include "Object/Object.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Collision/WorldPrimitivePickingBVH.h"
#include "GameFramework/AActor.h"
#include "GameFramework/CameraManager.h"
#include "GameFramework/Level.h"
#include "Component/CameraComponent.h"
#include "GameFramework/WorldContext.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/LODContext.h"
#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>
#include "Physics/IPhysicsScene.h"
#include <memory>

class UCameraComponent;
class UPrimitiveComponent;
class AGameModeBase;
class AGameStateBase;
class APlayerController;
class UClass;

class UWorld : public UObject {
public:
	DECLARE_CLASS(UWorld, UObject)
	UWorld() = default;
	~UWorld() override;

	// --- 월드 타입 ---
	EWorldType GetWorldType() const { return WorldType; }
	void SetWorldType(EWorldType InType) { WorldType = InType; }

	// 월드 복제 — 자체 Actor 리스트를 순회하며 각 Actor를 새 World로 Duplicate.
	// UObject::Duplicate는 Serialize 왕복만 수행하므로 UWorld처럼 컨테이너 기반 상태가 있는
	// 타입은 별도 오버라이드가 필요하다.
	UObject* Duplicate(UObject* NewOuter = nullptr) const override;

	// 지정된 WorldType으로 복제 — Actor 복제 전에 WorldType이 설정되므로
	// EditorOnly 컴포넌트의 CreateRenderState()에서 올바르게 판별 가능.
	UWorld* DuplicateAs(EWorldType InWorldType) const;

	// Actor lifecycle
	template<typename T>
	T* SpawnActor();
	// UClass 기반 spawn — 런타임에 클래스가 결정되는 경우(GameMode/GameState 등) 사용.
	AActor* SpawnActorByClass(UClass* Class);
	void DestroyActor(AActor* Actor);
	void AddActor(AActor* Actor);
	void MarkWorldPrimitivePickingBVHDirty();
	void BuildWorldPrimitivePickingBVHNow() const;
	void BeginDeferredPickingBVHUpdate();
	void EndDeferredPickingBVHUpdate();
	void WarmupPickingData() const;
	bool RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const;

	const TArray<AActor*>& GetActors() const { return PersistentLevel->GetActors(); }

	// LOD 컨텍스트를 FFrameContext에 전달 (Collect 단계에서 LOD 인라인 갱신용)
	FLODUpdateContext PrepareLODContext();

	void InitWorld();      // Set up the world before gameplay begins
	void BeginPlay();      // Triggers BeginPlay on all actors
	void Tick(float DeltaTime, ELevelTick TickType);  // Drives the game loop every frame
	void EndPlay();        // Cleanup before world is destroyed

	bool HasBegunPlay() const { return bHasBegunPlay; }

	// Active Camera — EditorViewportClient 또는 PlayerController가 세팅
	void SetActiveCamera(UCameraComponent* InCamera) { if (CameraManager) CameraManager->SetActiveCamera(InCamera); }
	UCameraComponent* GetActiveCamera() const { return CameraManager ? CameraManager->GetActiveCamera() : nullptr; }

	UCameraManager* GetCameraManager() const { return CameraManager; }

	// FScene — 렌더 프록시 관리자
	FScene& GetScene() { return Scene; }
	const FScene& GetScene() const { return Scene; }
	
	FSpatialPartition& GetPartition() { return Partition; }
	const FOctree* GetOctree() const { return Partition.GetOctree(); }
	void InsertActorToOctree(AActor* actor);
	void RemoveActorToOctree(AActor* actor);
	void UpdateActorInOctree(AActor* actor);

private:
	//TArray<AActor*> Actors;
	ULevel* PersistentLevel;

	UCameraManager* CameraManager = nullptr;
	UCameraComponent* LastLODUpdateCamera = nullptr;
	EWorldType WorldType = EWorldType::Editor;
	bool bHasBegunPlay = false;
	bool bHasLastFullLODUpdateCameraPos = false;
	mutable FWorldPrimitivePickingBVH WorldPrimitivePickingBVH;
	int32 DeferredPickingBVHUpdateDepth = 0;
	bool bDeferredPickingBVHDirty = false;
	uint32 VisibleProxyBuildFrame = 0;
	FVector LastFullLODUpdateCameraForward = FVector(1, 0, 0);
	FVector LastFullLODUpdateCameraPos = FVector(0, 0, 0);
	FScene Scene;
	FTickManager TickManager;

	FSpatialPartition Partition;
	std::unique_ptr<IPhysicsScene> PhysicsScene;

	// Game flow — Editor 월드에서는 nullptr로 유지된다.
	AGameModeBase* GameMode = nullptr;
	UClass* GameModeClass = nullptr;  // GameEngine 등이 BeginPlay 전에 세팅

public:
	IPhysicsScene* GetPhysicsScene() const { return PhysicsScene.get(); }

	// Physics raycast convenience — delegates to IPhysicsScene::Raycast
	bool PhysicsRaycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit) const;

	// --- Game flow ---
	// BeginPlay 이전에 호출. WorldType이 Editor면 무시된다.
	void SetGameModeClass(UClass* InClass) { GameModeClass = InClass; }
	AGameModeBase* GetGameMode() const { return GameMode; }
	AGameStateBase* GetGameState() const;
	APlayerController* GetFirstPlayerController() const;
};

template<typename T>
inline T* UWorld::SpawnActor()
{
	// create and register an actor
	T* Actor = UObjectManager::Get().CreateObject<T>(PersistentLevel);
	AddActor(Actor); // BeginPlay 트리거는 AddActor 내부에서 bHasBegunPlay 가드로 처리
	return Actor;
}
