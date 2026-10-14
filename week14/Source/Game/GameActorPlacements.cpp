#include "Game/GameActorPlacements.h"

#include "Engine/Runtime/ActorPlacementRegistry.h"
#include "Engine/Runtime/EngineInitHooks.h"
#include "GameFramework/Actor/CurvedWallRunColliderActor.h"
#include "GameFramework/World.h"

// ============================================================
// 게임-특화 액터를 Editor 의 "Place Actor" 메뉴에 등록 — 현재는 비어 있음.
//
// game-specific actor (전용 Pawn / NPC / spawner 등) 도입 시 여기에 RegisterEntry
// 항목을 추가한다. Engine 측은 이 함수의 이름만 알고 호출 — 새 액터 클래스 헤더는
// 이 cpp 안에서만 include 하면 됨.
// ============================================================
void RegisterGameActorPlacements()
{
	FActorPlacementRegistry::Get().RegisterEntry(
		"Curved Wall-Run Collider",
		[](UWorld* World, const FVector& Location) -> AActor*
		{
			if (!World) return nullptr;

			ACurvedWallRunColliderActor* Actor = World->SpawnActor<ACurvedWallRunColliderActor>();
			if (!Actor) return nullptr;

			// 에디터 배치 직후에도 곡선 query component와 WallRunnable 태그가 준비된 상태.
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(Location);
			return Actor;
		});
}

// 자기-등록 — Editor / Game 측이 함수명을 모르고도 FEngineInitHooks::RunAll() 로 호출됨.
namespace
{
	struct GameActorPlacementsAutoReg
	{
		GameActorPlacementsAutoReg() { FEngineInitHooks::Register(&RegisterGameActorPlacements); }
	};

	static GameActorPlacementsAutoReg gAutoReg;
}
