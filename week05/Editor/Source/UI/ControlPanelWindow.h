#pragma once
#include "CoreMinimal.h"

class FEngineRuntime;

class CControlPanelWindow
{
public:
	void Render(FEngineRuntime* Runtime);

private:
	TArray<FString> SceneFiles;
	int32 SelectedSceneIndex = -1;
};
