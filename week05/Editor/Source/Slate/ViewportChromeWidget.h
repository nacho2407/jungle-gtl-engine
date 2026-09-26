#pragma once

#include "Widget/Widget.h"
#include "Widget/Toolbar.h"
#include "ViewportToolbar.h"
#include "FpsStatWidget.h"

class FEditorEngine;
class FEditorUI;
class FEditorViewportClient;

class SViewportChromeWidget : public SWidget
{
public:
	SViewportChromeWidget(FEditorEngine* InEngine, FEditorUI* InEditorUI, FEditorViewportClient* InViewportClient, FViewportId InViewportId);

	FVector2 ComputeDesiredSize() const override;
	FVector2 ComputeMinSize() const override;
	void ArrangeChildren() override;
	FRect GetPaintClipRect() const override
	{
		return UnionRect(Rect, Toolbar.GetInteractiveRect());
	}
	void OnPaint(FSlatePaintContext& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;
	bool HitTest(FPoint Point) const override;

private:
	bool ShouldShowFPS() const;
	void SyncState();

private:
	FEditorUI* EditorUI = nullptr;
	SViewportToolbarWidget ViewSettings;
	FpsStatWidget FpsWidget;
	SToolbar Toolbar;

	uint32 BackgroundFillColor = 0xD01C1E21;
	uint32 BorderColor = 0xFF555B63;
};

