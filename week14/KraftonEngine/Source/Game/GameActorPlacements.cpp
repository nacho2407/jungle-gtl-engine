#include "Game/GameActorPlacements.h"

#include "Engine/Runtime/ActorPlacementRegistry.h"
#include "Engine/Runtime/EngineInitHooks.h"

// ============================================================
// 게임-특화 액터를 Editor 의 "Place Actor" 메뉴에 등록 — 현재는 비어 있음.
//
// game-specific actor (전용 Pawn / NPC / spawner 등) 도입 시 여기에 RegisterEntry
// 항목을 추가한다. Engine 측은 이 함수의 이름만 알고 호출 — 새 액터 클래스 헤더는
// 이 cpp 안에서만 include 하면 됨.
// ============================================================
void RegisterGameActorPlacements()
{
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
