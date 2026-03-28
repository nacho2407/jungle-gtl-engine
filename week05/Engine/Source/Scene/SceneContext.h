#pragma once

#include "CoreMinimal.h"
#include "Scene/WorldTypes.h"

class AActor;
class UScene;

struct ENGINE_API FSceneContext
{
	FString ContextName;
	EWorldType WorldType = EWorldType::Game;
	UScene* Scene = nullptr;

	bool IsValid() const { return Scene != nullptr; }
	void Reset()
	{
		ContextName.clear();
		WorldType = EWorldType::Game;
		Scene = nullptr;
	}
};

struct ENGINE_API FEditorSceneContext : public FSceneContext
{
	void Reset()
	{
		FSceneContext::Reset();
	}
};
