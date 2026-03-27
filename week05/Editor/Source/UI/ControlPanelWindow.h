#pragma once
#include "CoreMinimal.h"

class FEngine;

class CControlPanelWindow
{
public:
	void Render(FEngine* Engine);

private:
	TArray<FString> SceneFiles;
	int32 SelectedSceneIndex = -1;
};
