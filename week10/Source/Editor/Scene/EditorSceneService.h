#pragma once

#include "Core/CoreMinimal.h"

class UEditorEngine;

struct FEditorSceneOperationResult
{
	bool bSuccess = false;
	FString Message;
	FString ScenePath;
};

class FEditorSceneService
{
public:
	void Initialize(UEditorEngine* InEditorEngine);

	FEditorSceneOperationResult NewScene();
	FEditorSceneOperationResult OpenScene(const FString& FilePath, bool bPromptSave = true);
	FEditorSceneOperationResult SaveScene();
	FEditorSceneOperationResult SaveSceneToFilePath(const FString& FilePath);
	FEditorSceneOperationResult CreateSceneAsset(const FString& FilePath);
	FEditorSceneOperationResult RestoreLastScene();

	bool PromptSaveIfDirty();
	void MarkDirty() { bSceneDirty = true; }
	void ClearDirty() { bSceneDirty = false; }
	bool IsDirty() const { return bSceneDirty; }

	const FString& GetCurrentScenePath() const { return CurrentSceneFilePath; }
	bool HasCurrentScenePath() const { return !CurrentSceneFilePath.empty(); }
	const char* GetSceneName() const { return SceneName; }
	FString GetCurrentSceneDisplayPath() const;

private:
	bool PromptSaveSceneAs(FString& OutFilePath) const;
	void SetCurrentScenePath(const FString& FilePath);
	void ClearCurrentScenePath();
	void PushFooterLog(const FString& Message) const;
	FEditorSceneOperationResult MakeResult(bool bSuccess, const FString& Message, const FString& ScenePath = {}) const;

private:
	UEditorEngine* EditorEngine = nullptr;
	char SceneName[128] = "Untitled";
	FString CurrentSceneFilePath;
	bool bSceneDirty = false;
};
