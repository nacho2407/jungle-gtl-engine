#pragma once

#include "CoreMinimal.h"
#include "Level/WorldTypes.h"

class AActor;
class ULevel;

struct ENGINE_API FLevelContext
{
	FString ContextName;
	EWorldType WorldType = EWorldType::Game;
	ULevel* Scene = nullptr;

	bool IsValid() const { return Scene != nullptr; }
	void Reset()
	{
		ContextName.clear();
		WorldType = EWorldType::Game;
		Scene = nullptr;
	}
};

struct ENGINE_API FEditorLevelContext : public FLevelContext
{
	void Reset()
	{
		FLevelContext::Reset();
	}
};
