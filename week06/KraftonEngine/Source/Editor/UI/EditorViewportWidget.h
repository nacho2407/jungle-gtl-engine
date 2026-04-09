#pragma once

#include "Editor/UI/EditorWidget.h"
#include <string>

class FLevelEditorViewportClient;

class FEditorViewportWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) override;

	void SetViewportClient(FLevelEditorViewportClient* InClient) { ViewportClient = InClient; }
	void SetIndex(int32 InIndex);

private:
	FLevelEditorViewportClient* ViewportClient = nullptr;
	int32 Index = 0;
	std::string WindowName = "Viewport";
};
