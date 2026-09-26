#include "EditorViewportOverlay.h"

SEditorViewportOverlay::SEditorViewportOverlay(
	FEditorEngine* InEngine,
	FEditorUI* InEditorUI,
	FEditorViewportClient* InViewportClient)
{
	GlobalToolbar = std::make_unique<SLayoutToolbarWidget>(InEngine, InViewportClient);
	for (int32 ViewportId = 0; ViewportId < MAX_VIEWPORTS; ++ViewportId)
	{
		ViewportToolbars[ViewportId] = std::make_unique<SViewportChromeWidget>(InEngine, InEditorUI, InViewportClient, ViewportId);
	}
}

SWidget* SEditorViewportOverlay::GetGlobalChromeWidget()
{
	return GlobalToolbar.get();
}

SWidget* SEditorViewportOverlay::GetViewportChromeWidget(FViewportId ViewportId)
{
	return (ViewportId >= 0 && ViewportId < MAX_VIEWPORTS) ? ViewportToolbars[ViewportId].get() : nullptr;
}
