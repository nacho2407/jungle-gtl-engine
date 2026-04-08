#pragma once

#include "Widget/Widget.h"
#include "Widget/Toolbar.h"
#include "TransformWidget.h"

class FEditorEngine;
class FEditorViewportClient;

class SLayoutToolbarWidget : public SWidget
{
public:
	explicit SLayoutToolbarWidget(FEditorEngine* InEngine, FEditorViewportClient* InViewportClient);

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
	FEditorEngine* Engine = nullptr;
	SToolbar Toolbar;
	FTransformWidget TransformWidget;
};

