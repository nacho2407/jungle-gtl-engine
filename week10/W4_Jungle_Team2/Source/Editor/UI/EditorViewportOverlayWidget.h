#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorViewportOverlayWidget : public FEditorWidget
{
private:
	bool bExpanded = false;
	bool bShowShortcutsWindow = true;
	void RenderViewportSettings(float DeltaTime);
	void RenderDebugStats(float DeltaTime);
	void RenderSplitterBar();
	void RenderBoxSelectionOverlay();
	void RenderShortcutsWindow();

public:
	void Render(float DeltaTime) override;
};
