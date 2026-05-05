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
#include "GameFramework/PlayerController.h"
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
	NewWorld->WorldSettings = WorldSettings;  // 씬 단위 설정 (GameMode 등) 복제
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
	NewWorld->WorldSettings = WorldSettings;  // 씬 단위 설정 복제
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
	if (!Actor) return;

	Actor->EndPlay();
	PersistentLevel->RemoveActor(Actor);

	MarkWorldPrimitivePickingBVHDirty();
	Partition.RemoveActor(Actor);

	// 즉시 delete. octree / partition 측은 IsValid 가드로 stale 포인터 방어.
	// PhysX onContact / TickManager 순회 도중에 self-destroy 하는 경로 (police HandleHit,
	// meteor Tick) 는 호출자 측에서 stack 위쪽 코드가 더 이상 this 를 만지지 않도록
	// 패턴화해뒀음 (bAlreadyCaught 가드, ElapsedTime=Lifetime 후 다음 Tick).
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

// PC 의 PlayerCameraManager 우선, fallback 으로 IPOVProvider pull.
bool UWorld::GetActivePOV(FMinimalViewInfo& OutPOV) const
{
	if (APlayerController* PC = GetFirstPlayerController())
	{
		if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
		{
			if (CM->GetCameraView(OutPOV))
			{
				return true;
			}
		}
	}
	if (EditorPOVProvider)
	{
		return EditorPOVProvider->GetCameraView(OutPOV);
	}
	return false;
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
	// 잔여 정리: POV currency 사용. 카메라 인스턴스 비교는 제거 — 위치/회전 변화로만 swap 감지.
	// 거의 같은 위치로 카메라가 swap 되면 한 프레임 stale LOD 가능하지만 다음 프레임 회복.
	FMinimalViewInfo POV;
	if (!GetActivePOV(POV)) return {};

	const FVector CameraPos = POV.Location;
	const FVector CameraForward = POV.Rotation.GetForwardVector();

	const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
	const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
	const bool bShouldStaggerLOD = Scene.GetProxyCount() >= LOD_STAGGER_MIN_VISIBLE;

	const bool bForceFullLODRefresh =
		!bShouldStaggerLOD
		|| !bHasLastFullLODUpdateCameraPos
		|| FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ
		|| CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

	if (bForceFullLODRefresh)
	{
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

	// E.2/3: CameraManager spawn 은 PC 의 BeginPlay 가 담당. World 는 보유하지 않음.

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

	// E.2/3: AutoPossessDefaultCamera 는 PC 의 BeginPlay 가 처리.
}

void UWorld::Tick(float DeltaTime, ELevelTick TickType)
{
	{
		SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
		Partition.FlushPrimitive();
	}

	Scene.GetDebugDrawQueue().Tick(DeltaTime);

	// CameraManager 갱신 — Shake / Fade / ViewTarget blend.
	// bPaused 가드 위에 둬서 메뉴/모달 중에도 fade 트랜지션이 진행되도록 함
	// (UE 의 PlayerCameraManager 도 일시정지와 무관하게 tick).
	// TODO: GlobalTimeDilation 도입 시 raw(unpaused) dt 를 별도로 받아 셰이크가
	//       Slomo 영향을 받지 않게 할 것.
	// E.2/3: PC 의 PlayerCameraManager 를 Tick.
	if (APlayerController* PC = GetFirstPlayerController())
	{
		if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
		{
			CM->UpdateCamera(DeltaTime);
		}
	}

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
