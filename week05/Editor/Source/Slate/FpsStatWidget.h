#pragma once

#include "Widget/TextBlock.h"

class FEditorEngine;

class FpsStatWidget : public SWidget
{
public:
	FpsStatWidget(FEditorEngine* InEngine);

	void OnPaint(FSlatePaintContext& Painter) override;
	bool HitTest(FPoint Point) const override { (void)Point; return false; }
	FVector2 ComputeDesiredSize() const override { return bVisible ? FVector2{ static_cast<float>(GetDesiredWidth()), 24.0f } : FVector2{ 0.0f, 0.0f }; }
	void SetWidgetRect(const FRect& InRect);
	void Refresh();
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }
	int32 GetDesiredWidth() const;

private:
	void UpdateGeometry();
	void SyncValue();

private:
	FEditorEngine* Engine = nullptr;
	float FontSize = 16.0f;
	float FPS = 0.00f;
	float FrameTimeMs = 0.0f;
	STextBlock FpsTextBlock;
	const int32 Gap = 16;
	bool bVisible = true;
};

