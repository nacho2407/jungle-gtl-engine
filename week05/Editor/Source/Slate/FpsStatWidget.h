#pragma once

#include "Widget/TextBlock.h"

class FEditorEngine;

class FpsStatWidget : public SWidget
{
public:
	FpsStatWidget(FEditorEngine* InEngine);

	void OnPaint(SWidget& Painter) override;
	bool HitTest(FPoint Point) const override { (void)Point; return false; }
	void SetWidgetRect(const FRect& InRect);
	void Refresh();
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
};
