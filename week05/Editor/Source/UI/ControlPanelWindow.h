#pragma once
#include "CoreMinimal.h"

class FEditorEngine;

class FControlPanelWindow
{
public:
	void Render(FEditorEngine* Engine);

private:
	TArray<FString> SceneFiles;
	int32 SelectedSceneIndex = -1;
};
