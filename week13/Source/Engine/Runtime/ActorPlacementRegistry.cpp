#include "Engine/Runtime/ActorPlacementRegistry.h"

FActorPlacementRegistry& FActorPlacementRegistry::Get()
{
	static FActorPlacementRegistry Instance;
	return Instance;
}

void FActorPlacementRegistry::RegisterEntry(const FString& Label, FSpawnFn SpawnFn)
{
	if (!SpawnFn) return;
	Entries.push_back({ Label, std::move(SpawnFn) });
}
