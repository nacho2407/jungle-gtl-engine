#pragma once
#include "CoreMinimal.h"

class CCore;

class CControlPanelWindow
{
public:
	void Render(CCore* Core);

private:
	TArray<FString> SceneFiles;
	int32 SelectedSceneIndex = -1;
};
