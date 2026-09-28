#pragma once
#include "ObjViewerWidget.h"
#include "Engine/Core/Common.h"

class FObjViewerMenuBarWidget : public FObjViewerWidget
{
public:
	virtual void Render(float DeltaTime) override;

private:
	FString OpenFileDialog(const wchar_t* Filter);
	FString SaveFileDialog(const wchar_t* Filter, const wchar_t* DefExt = nullptr);

	void LoadObj();
	void LoadScene();

	TArray<FString> SceneFiles;
	char SceneName[128] = "Default";
};