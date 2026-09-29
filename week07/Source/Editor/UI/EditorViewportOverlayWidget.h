#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorViewportOverlayWidget : public FEditorWidget
{
private:
	bool bExpanded = false;
	bool bShowViewportSettings = true;
	bool bShowShortcutsWindow = false;
	void RenderViewportSettings(float DeltaTime);
	void RenderDebugStats(float DeltaTime);
	void RenderSplitterBar();
	void RenderBoxSelectionOverlay();
	void RenderShortcutsWindow();

public:
	bool IsViewportSettingsVisible() const { return bShowViewportSettings; }
	void SetViewportSettingsVisible(bool bVisible) { bShowViewportSettings = bVisible; }
	bool IsShortcutsWindowVisible() const { return bShowShortcutsWindow; }
	void SetShortcutsWindowVisible(bool bVisible) { bShowShortcutsWindow = bVisible; }
	void Render(float DeltaTime) override;
};
