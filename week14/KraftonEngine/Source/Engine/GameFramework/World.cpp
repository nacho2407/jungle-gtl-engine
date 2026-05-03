#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Component/CameraComponent.h"
#include "Render/Types/LODContext.h"
#include "Physics/NativePhysicsScene.h"
#include "Physics/PhysXPhysicsScene.h"
#include "Core/ProjectSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Object/UClass.h"
#include <algorithm>
#include "Profiling/Stats.h"

IMPLEMENT_CLASS(UWorld, UObject)


UWorld::~UWorld()
{
	if (PersistentLevel && !PersistentLevel->GetActors().empty())
	{
		EndPlay();
	}
}

UObject* UWorld::Duplicate(UObject* NewOuter) const
{
	// UE의 CreatePIEWorldByDuplication 대응 (간소화 버전).
	// 새 UWorld를 만들고, 소스의 Actor들을 하나씩 복제해 NewWorld를 Outer로 삼아 등록한다.
	// AActor::Duplicate 내부에서 Dup->GetTypedOuter<UWorld>() 경유 AddActor가 호출되므로
	// 여기서는 World 단위 상태만 챙기면 된다.
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!NewWorld)
	{
		return nullptr;
	}
	NewWorld->SetOuter(NewOuter);
	NewWorld->InitWorld(); // Partition/VisibleSet 초기화 — 이거 없으면 복제 액터가 렌더링되지 않음

	for (AActor* Src : GetActors())
	{
		if (!Src) continue;
		Src->Duplicate(NewWorld);
	}

	NewWorld->PostDuplicate();
	return NewWorld;
}

UWorld* UWorld::DuplicateAs(EWorldType InWorldType) const
{
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!NewWorld) return nullptr;

	NewWorld->SetWorldType(InWorldType);
	NewWorld->InitWorld();

	for (AActor* Src : GetActors())
	{
		if (!Src) continue;
		Src->Duplicate(NewWorld);
	}

	NewWorld->PostDuplicate();
	return NewWorld;
}

void UWorld::DestroyActor(AActor* Actor)
{
	// remove and clean up
	if (!Actor) return;
	Actor->EndPlay();
	// Remove from actor list
	PersistentLevel->RemoveActor(Actor);

	MarkWorldPrimitivePickingBVHDirty();
	Partition.RemoveActor(Actor);

	// Mark for garbage collection
	UObjectManager::Get().DestroyObject(Actor);
}

AActor* UWorld::SpawnActorByClass(UClass* Class)
{
	if (!Class) return nullptr;

	UObject* Created = FObjectFactory::Get().Create(Class->GetName(), PersistentLevel);
	AActor* Actor = Cast<AActor>(Created);
	if (!Actor) return nullptr;

	AddActor(Actor);
	return Actor;
}

AGameStateBase* UWorld::GetGameState() const
{
	return GameMode ? GameMode->GetGameState() : nullptr;
}

APlayerController* UWorld::GetFirstPlayerController() const
{
	return GameMode ? GameMode->GetPlayerController() : nullptr;
}

void UWorld::AddActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	PersistentLevel->AddActor(Actor);

	InsertActorToOctree(Actor);
	MarkWorldPrimitivePickingBVHDirty();

	// PIE 중 Duplicate(Ctrl+D)나 SpawnActor로 들어온 액터에도 BeginPlay를 보장.
	if (bHasBegunPlay && !Actor->HasActorBegunPlay())
	{
		Actor->BeginPlay();
	}
}

void UWorld::MarkWorldPrimitivePickingBVHDirty()
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.MarkDirty();
}

void UWorld::BuildWorldPrimitivePickingBVHNow() const
{
	WorldPrimitivePickingBVH.BuildNow(GetActors());
}

void UWorld::BeginDeferredPickingBVHUpdate()
{
	++DeferredPickingBVHUpdateDepth;
}

void UWorld::EndDeferredPickingBVHUpdate()
{
	if (DeferredPickingBVHUpdateDepth <= 0)
	{
		return;
	}

	--DeferredPickingBVHUpdateDepth;
	if (DeferredPickingBVHUpdateDepth == 0 && bDeferredPickingBVHDirty)
	{
		bDeferredPickingBVHDirty = false;
		BuildWorldPrimitivePickingBVHNow();
	}
}

void UWorld::WarmupPickingData() const
{
	for (AActor* Actor : GetActors())
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive || !Primitive->IsVisible() || !Primitive->IsA<UStaticMeshComponent>())
			{
				continue;
			}

			UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Primitive);
			if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
			{
				StaticMesh->EnsureMeshTrianglePickingBVHBuilt();
			}
		}
	}

	BuildWorldPrimitivePickingBVHNow();
}

bool UWorld::RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	//혹시라도 BVH 트리가 업데이트 되지 않았다면 업데이트
	WorldPrimitivePickingBVH.EnsureBuilt(GetActors());
	return WorldPrimitivePickingBVH.Raycast(Ray, OutHitResult, OutActor);
}

bool UWorld::PhysicsRaycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	if (PhysicsScene)
		return PhysicsScene->Raycast(Start, Dir, MaxDist, OutHit, TraceChannel, IgnoreActor);
	return false;
}


void UWorld::InsertActorToOctree(AActor* Actor)
{
	Partition.InsertActor(Actor);
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
	Partition.RemoveActor(Actor);
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
	Partition.UpdateActor(Actor);
}

FLODUpdateContext UWorld::PrepareLODContext()
{
	UCameraComponent* ActiveCamera = GetActiveCamera();

	if (!ActiveCamera) return {};

	const FVector CameraPos = ActiveCamera->GetWorldLocation();
	const FVector CameraForward = ActiveCamera->GetForwardVector();

	const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
	const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
	const bool bShouldStaggerLOD = Scene.GetProxyCount() >= LOD_STAGGER_MIN_VISIBLE;

	const bool bForceFullLODRefresh =
		!bShouldStaggerLOD
		|| LastLODUpdateCamera != ActiveCamera
		|| !bHasLastFullLODUpdateCameraPos
		|| FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ
		|| CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

	if (bForceFullLODRefresh)
	{
		LastLODUpdateCamera = ActiveCamera;
		LastFullLODUpdateCameraPos = CameraPos;
		LastFullLODUpdateCameraForward = CameraForward;
		bHasLastFullLODUpdateCameraPos = true;
	}

	FLODUpdateContext Ctx;
	Ctx.CameraPos = CameraPos;
	Ctx.LODUpdateFrame = LODUpdateFrame;
	Ctx.LODUpdateSlice = LODUpdateSlice;
	Ctx.bForceFullRefresh = bForceFullLODRefresh;
	Ctx.bValid = true;
	return Ctx;
}

void UWorld::InitWorld()
{
	Partition.Reset(FBoundingBox());
	PersistentLevel = UObjectManager::Get().CreateObject<ULevel>(this);
	PersistentLevel->SetWorld(this);

	CameraManager = UObjectManager::Get().CreateObject<UCameraManager>(this);

	// 물리 시스템 초기화 — ProjectSettings 백엔드 선택
	if (FProjectSettings::Get().Physics.Backend == EPhysicsBackend::PhysX)
		PhysicsScene = std::make_unique<FPhysXPhysicsScene>();
	else
		PhysicsScene = std::make_unique<FNativePhysicsScene>();
	PhysicsScene->Initialize(this);
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;

	// GameMode spawn — Editor 월드에서는 생성하지 않는다.
	// Level::BeginPlay 이전에 spawn하면 그 루프에서 GameMode/GameState도 BeginPlay된다.
	if (WorldType != EWorldType::Editor && GameModeClass)
	{
		AActor* Spawned = SpawnActorByClass(GameModeClass);
		GameMode = Cast<AGameModeBase>(Spawned);
	}

	if (PersistentLevel)
	{
		PersistentLevel->BeginPlay();
	}

	// 모든 액터 BeginPlay 완료 후 매치 시작 — 페이즈 변경 브로드캐스트가
	// 이때 모든 리스너에게 안전하게 도달한다.
	if (GameMode)
	{
		GameMode->StartMatch();
	}

	// BeginPlay에서 CameraComponent의 register 이후 Possess 시도
	if (CameraManager)
	{
		CameraManager->AutoPossessDefaultCamera();
	}
}

void UWorld::Tick(float DeltaTime, ELevelTick TickType)
{
	{
		SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
		Partition.FlushPrimitive();
	}

	Scene.GetDebugDrawQueue().Tick(DeltaTime);

	// bPaused 동안 PhysicsScene + TickManager skip — GameMode 타이머, Lua Tick, 차량
	// 이동, PhysX 시뮬레이션 모두 정지. Render / UI / Input poll 은 호출자 (UEngine::Tick)
	// 가 따로 돌리므로 영향 없음 → 메뉴/인트로 위에서 화면 보이고 클릭 가능.
	if (bPaused)
	{
		return;
	}

	if (bHasBegunPlay && PhysicsScene)
	{
		SCOPE_STAT_CAT("PhysicsScene", "1_WorldTick");
		PhysicsScene->Tick(DeltaTime);
	}

	TickManager.Tick(this, DeltaTime, TickType);
}

void UWorld::EndPlay()
{
	if (GameMode)
	{
		GameMode->EndMatch();
	}

	bHasBegunPlay = false;
	TickManager.Reset();

	if (!PersistentLevel)
	{
		return;
	}

	PersistentLevel->EndPlay();

	// 물리 시스템 정리 — 액터/컴포넌트가 아직 살아있는 동안 해제
	if (PhysicsScene)
	{
		PhysicsScene->Shutdown();
	}

	// Clear spatial partition while actors/components are still alive.
	// Otherwise Octree teardown can dereference stale primitive pointers during shutdown.
	Partition.Reset(FBoundingBox());

	PersistentLevel->Clear();
	GameMode = nullptr; // 액터 리스트가 비워지면서 dangling 되므로 명시적으로 해제
	MarkWorldPrimitivePickingBVHDirty();

	// PersistentLevel은 CreateObject로 생성되었으므로 DestroyObject로 해제해야 alloc count가 맞음
	UObjectManager::Get().DestroyObject(PersistentLevel);
	PersistentLevel = nullptr;
}
