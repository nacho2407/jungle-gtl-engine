#pragma once

#include "Widget/Widget.h"
#include "Widget/Button.h"

class FEditorEngine;
class FEditorViewportClient;

class FTransformWidget : public SWidget
{
public:
	FTransformWidget(FEditorEngine* InEngine, FEditorViewportClient* InViewportClient);

	void OnPaint(SWidget& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;
	bool HitTest(FPoint Point) const override;
	void SetWidgetRect(const FRect& InRect);
	FRect GetInteractiveRect() const;
	int32 GetDesiredWidth() const;
	int32 GetRightPadding() const { return Padding; }

private:
	void SyncSelectionState();
	void UpdateGeometry();

	void SetTranslateMode();
	void SetRotationMode();
	void SetScaleMode();
	void ToggleCoordMode();

	bool HandleButtonMouse(SButton& Button, int32 X, int32 Y);

	FRect GetExpandedInteractiveRect() const;

	static bool ContainsPoint(const FRect& InRect, FPoint Point);
	static FRect UnionRects(const FRect& A, const FRect& B);

private:
	FEditorEngine* Engine = nullptr;
	FEditorViewportClient* ViewportClient = nullptr;

	int32 ButtonSize = 24;
	int32 Padding = 8;
	int32 Gap = 6;

	SButton TranslateModeButton;
	SButton RotationModeButton;
	SButton ScaleModeButton;
	SButton ToggleCoordModeButton;
};
