#pragma once

#include "Widget/Widget.h"
#include "Widget/SViewport.h"
#include <algorithm>

class SViewportHost : public SWidget
{
public:
	void SetViewportWidget(SViewport* InViewportWidget) { ViewportWidget = InViewportWidget; }
	void SetHeaderHeight(int32 InHeaderHeight) { HeaderHeight = InHeaderHeight; }

	FRect GetHeaderRect() const { return HeaderRect; }
	FRect GetSceneRect() const { return ViewportWidget ? ViewportWidget->Rect : FRect{ 0, 0, 0, 0 }; }
	SViewport* GetViewportWidget() const { return ViewportWidget; }

	FVector2 ComputeDesiredSize() const override { return { 320.0f, 240.0f + static_cast<float>(HeaderHeight) }; }
	FVector2 ComputeMinSize() const override;
	void ArrangeChildren() override;
	void OnPaint(FSlatePaintContext& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;

	void SetMinSceneSize(int32 InWidth, int32 InHeight)
	{
		MinSceneWidth = (std::max)(0, InWidth);
		MinSceneHeight = (std::max)(0, InHeight);
	}

private:
	int32 HeaderHeight = 34;
	int32 MinSceneWidth = 160;
	int32 MinSceneHeight = 120;
	FRect HeaderRect;
	SViewport* ViewportWidget = nullptr;
};
