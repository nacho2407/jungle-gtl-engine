#pragma once

#include "CoreMinimal.h"
#include "Scene/SceneTypes.h"

class AActor;
class UScene;

struct ENGINE_API FSceneContext
{
	FString ContextName;
	ESceneType SceneType = ESceneType::Game;
	UScene* Scene = nullptr;

	bool IsValid() const { return Scene != nullptr; }
	void Reset()
	{
		ContextName.clear();
		SceneType = ESceneType::Game;
		Scene = nullptr;
	}
};

struct ENGINE_API FEditorSceneContext : public FSceneContext
{
	TObjectPtr<AActor> SelectedActor;

	void Reset()
	{
		FSceneContext::Reset();
		SelectedActor = nullptr;
	}
};
