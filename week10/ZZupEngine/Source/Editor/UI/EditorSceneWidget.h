#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/CoreMinimal.h"

class FEditorSceneWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;
	void NewScene();
	void SaveScene();
	void LoadScene();
	void SaveSceneToFilePath(const FString& FilePath);
	void LoadSceneFromFilePath(const FString& FilePath);
	void RefreshSceneAndAssets();

private:
	void RefreshSceneFileList();

	char SceneName[128] = "Default";

	TArray<FString> SceneFiles;
	int32 SelectedSceneIndex = -1;

	float NewSceneNotificationTimer = 0.f;
	float SceneSaveNotificationTimer = 0.f;
	float SceneLoadNotificationTimer = 0.f;
};
