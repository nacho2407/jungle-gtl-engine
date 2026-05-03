#include "Game/GameActorPlacements.h"

#include "Engine/Runtime/ActorPlacementRegistry.h"
#include "Engine/Runtime/EngineInitHooks.h"
#include "GameFramework/World.h"
#include "Math/Vector.h"

#include "Game/Pawn/CarPawn.h"

void RegisterGameActorPlacements()
{
	FActorPlacementRegistry::Get().RegisterEntry(
		"Car Pawn",
		[](UWorld* World, const FVector& Location) -> AActor*
		{
			if (!World) return nullptr;
			ACarPawn* Actor = World->SpawnActor<ACarPawn>();
			if (!Actor) return nullptr;
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
