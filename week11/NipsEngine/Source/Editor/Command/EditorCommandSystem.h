#pragma once

#include "Core/CoreMinimal.h"

class UEditorEngine;

enum class EEditorCommand : uint8
{
	NewScene,
	OpenScene,
	SaveScene,
	SaveSceneAs,
	Undo,
	Redo,
	ClearUndoHistory,
	RestoreUndoHistoryIndex,
	RefreshAssets,
	StartPlay,
	StopPlay,
};

struct FEditorCommandArgs
{
	FString ScenePath;
	bool bPromptSave = false;
	int32 HistoryIndex = -1;
};

class FEditorCommandSystem
{
public:
	void Initialize(UEditorEngine* InEditorEngine);

	bool CanExecute(EEditorCommand Command, const FEditorCommandArgs& Args = {}) const;
	bool Execute(EEditorCommand Command, const FEditorCommandArgs& Args = {});

private:
	UEditorEngine* EditorEngine = nullptr;
};
