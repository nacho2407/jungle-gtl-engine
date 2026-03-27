#pragma once
#include "CoreMinimal.h"
#include "Scene/SceneTypes.h"

class AActor;
class UWorld;

struct ENGINE_API FWorldContext
{
	FString ContextName;
	ESceneType WorldType = ESceneType::Game;
	UWorld* World = nullptr;

	bool IsValid() const { return World != nullptr; }
	void Reset()
	{
		ContextName.clear();
		WorldType = ESceneType::Game;
		World = nullptr;
	}
};

struct ENGINE_API FEditorWorldContext : public FWorldContext
{
	TObjectPtr<AActor> SelectedActor;

	void Reset()
	{
		FWorldContext::Reset();
		SelectedActor = nullptr;
	}
};
