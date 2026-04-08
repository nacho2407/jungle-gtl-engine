#pragma once

#include "Widget/Widget.h"
#include "Widget/SViewport.h"

class SViewportHost : public SWidget
{
public:
	void SetViewportWidget(SViewport* InViewportWidget) { ViewportWidget = InViewportWidget; }
	void SetHeaderHeight(int32 InHeaderHeight) { HeaderHeight = InHeaderHeight; }

	FRect GetHeaderRect() const { return HeaderRect; }
	FRect GetSceneRect() const { return ViewportWidget ? ViewportWidget->Rect : FRect{ 0, 0, 0, 0 }; }
	SViewport* GetViewportWidget() const { return ViewportWidget; }

	FVector2 ComputeDesiredSize() const override { return { 320.0f, 240.0f + static_cast<float>(HeaderHeight) }; }
	void ArrangeChildren() override;
	void OnPaint(FSlatePaintContext& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;

private:
	int32 HeaderHeight = 34;
	FRect HeaderRect;
	SViewport* ViewportWidget = nullptr;
};

