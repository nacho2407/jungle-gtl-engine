#pragma once

#include "Widget/Widget.h"
#include "LayoutToolbarWidget.h"
#include "ViewportChromeWidget.h"
#include <memory>

class FEditorEngine;
class FEditorUI;
class FEditorViewportClient;

class SEditorViewportOverlay : public SWidget
{
public:
	SEditorViewportOverlay(FEditorEngine* InEngine, FEditorUI* InEditorUI, FEditorViewportClient* InViewportClient);

	bool IsChromeProvider() const override { return true; }
	SWidget* GetGlobalChromeWidget() override;
	SWidget* GetViewportChromeWidget(FViewportId ViewportId) override;

	void OnPaint(FSlatePaintContext& Painter) override { (void)Painter; }
	bool OnMouseDown(int32 X, int32 Y) override { (void)X; (void)Y; return false; }
	bool HitTest(FPoint Point) const override { (void)Point; return false; }

private:
	std::unique_ptr<SLayoutToolbarWidget> GlobalToolbar;
	std::unique_ptr<SViewportChromeWidget> ViewportToolbars[MAX_VIEWPORTS];
};

