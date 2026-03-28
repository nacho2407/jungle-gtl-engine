#pragma once
#include "CoreMinimal.h"
#include "Scene/WorldTypes.h"

class AActor;
class UWorld;

struct ENGINE_API FWorldContext
{
	FString ContextName;
	EWorldType WorldType = EWorldType::Game;
	UWorld* World = nullptr;

	bool IsValid() const { return World != nullptr; }
	void Reset()
	{
		ContextName.clear();
		WorldType = EWorldType::Game;
		World = nullptr;
	}
};
