#include "Game/GameActorPlacements.h"

#include "Engine/Runtime/ActorPlacementRegistry.h"
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
