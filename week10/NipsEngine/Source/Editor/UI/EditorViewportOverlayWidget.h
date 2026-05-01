#pragma once

#include "Editor/UI/EditorWidget.h"

struct ImDrawList;

class FEditorViewportOverlayWidget : public FEditorWidget
{
private:
	bool bExpanded = false;
	bool bShowViewportSettings = false;
	bool bShowShortcutsWindow = false;
	bool bShowGroupedStatOverlay = false;
	void RenderViewportSettings(float DeltaTime);
	void RenderDebugStats(float DeltaTime);
	void RenderGroupedStatOverlay(float DeltaTime);
    void RenderViewportFocusOverlay();
    void RenderBoxSelectionOverlay();
    void RenderShortcutsWindow();
    void RenderShadowAtlasPreview();
    void RenderShadowCubeArrayPreview();

public:
	bool IsViewportSettingsVisible() const { return bShowViewportSettings; }
	void SetViewportSettingsVisible(bool bVisible) { bShowViewportSettings = bVisible; }
	bool IsShortcutsWindowVisible() const { return bShowShortcutsWindow; }
	void SetShortcutsWindowVisible(bool bVisible) { bShowShortcutsWindow = bVisible; }
	bool IsGroupedStatOverlayVisible() const { return bShowGroupedStatOverlay; }
	void SetGroupedStatOverlayVisible(bool bVisible) { bShowGroupedStatOverlay = bVisible; }
    void Render(float DeltaTime) override;
    void RenderViewportFrameOverlays(float DeltaTime);
    void RenderFloatingOverlays(float DeltaTime);
	void RenderSplitterBar(ImDrawList* DrawList);

    bool bShowShadowAtlasGrid = true;
    bool bShowShadowAtlasZoom = true;
    float ShadowAtlasPreviewSize = 256.0f;
};
