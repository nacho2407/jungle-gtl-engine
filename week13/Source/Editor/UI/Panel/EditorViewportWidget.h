#pragma once

#include "Editor/UI/Panel/EditorPanelWidget.h"
#include <string>

class FLevelEditorViewportClient;

class FEditorViewportWidget : public FEditorPanelWidget
{
public:
	void Render(const FEditorPanelContext& Context) override;

	void SetViewportClient(FLevelEditorViewportClient* InClient) { ViewportClient = InClient; }
	void SetIndex(int32 InIndex);

private:
	FLevelEditorViewportClient* ViewportClient = nullptr;
	int32 Index = 0;
	std::string WindowName = "Viewport";
};
