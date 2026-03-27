#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/CoreTypes.h"

class FEditorSceneWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	void RefreshSceneFileList();

	char SceneName[128] = "Default";

	TArray<FString> SceneFiles;
	int32 SelectedSceneIndex = -1;

	float NewSceneNotificationTimer = 0.f;
	float SceneSaveNotificationTimer = 0.f;
	float SceneLoadNotificationTimer = 0.f;
};
