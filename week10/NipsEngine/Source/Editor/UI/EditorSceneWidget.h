#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/CoreMinimal.h"

class AActor;

class FEditorSceneWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;
	void NewScene();
	bool SaveScene();
	void LoadScene();
	bool SaveSceneToFilePath(const FString& FilePath);
	bool LoadSceneFromFilePath(const FString& FilePath, bool bPromptSave = true);
	void RefreshSceneAndAssets();
	FString GetCurrentSceneDisplayPath() const;
	bool HasCurrentSceneFilePath() const { return !CurrentSceneFilePath.empty(); }
	bool IsSceneDirty() const { return bSceneDirty; }
	bool PromptSaveIfDirty();
	void MarkSceneDirty() { bSceneDirty = true; }

private:
	bool PromptSaveSceneAs(FString& OutFilePath) const;
	void SetCurrentScenePath(const FString& FilePath);

private:
	char SceneName[128] = "Untitled";
	FString CurrentSceneFilePath;
	bool bSceneDirty = false;

	TArray<FString> SceneFiles;
	int32 LastClickedActorIndex = -1;
	int32 SelectedSceneIndex = -1;
	bool bOpenOutlinerContextMenu = false;
	bool bOpenRenameActorPopup = false;
	AActor* PendingRenameActor = nullptr;
	char OutlinerSearchText[128] = "";
	char RenameActorName[128] = "";

	float NewSceneNotificationTimer = 0.f;
	float SceneSaveNotificationTimer = 0.f;
	float SceneLoadNotificationTimer = 0.f;
};
