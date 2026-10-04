#pragma once

#include "Core/CoreMinimal.h"
#include "SimpleJSON/json.hpp"

class AActor;
class UWorld;

struct FActorLoadOptions
{
	bool bPreserveUUIDs = true;
	bool bPreserveName = true;
	bool bMakeNameUnique = false;
	bool bCallBeginPlayIfWorldBegunPlay = true;
};

namespace FActorSerialization
{
	json::JSON BuildActorJson(AActor* Actor);
	AActor* SpawnActorFromJson(UWorld* World, json::JSON& ActorData, const FActorLoadOptions& Options = FActorLoadOptions());
}
