#pragma once

#include "Widget/Widget.h"
#include "Widget/Button.h"

class FEditorEngine;
class FEditorViewportClient;

class FTransformWidget : public SWidget
{
public:
	FTransformWidget(FEditorEngine* InEngine, FEditorViewportClient* InViewportClient);

	void OnPaint(FSlatePaintContext& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;
	bool HitTest(FPoint Point) const override;
	FVector2 ComputeDesiredSize() const override { return { static_cast<float>(GetDesiredWidth()), static_cast<float>(ButtonSize) }; }
	FVector2 ComputeMinSize() const override { return { static_cast<float>(ButtonSize * 4), static_cast<float>(ButtonSize) }; }
	void SetWidgetRect(const FRect& InRect);
	FRect GetInteractiveRect() const;
	int32 GetDesiredWidth() const;
	int32 GetRightPadding() const { return Padding; }

	uint32 ActiveButtonBackgroundColor = 0xFF3B5E84;
	uint32 InactiveButtonBackgroundColor = 0xFF2C2F33;
	uint32 ActiveButtonBorderColor = 0xFF86C8FF;
	uint32 InactiveButtonBorderColor = 0xFF5A6068;
	uint32 ActiveButtonTextColor = 0xFFFFFFFF;
	uint32 InactiveButtonTextColor = 0xFFFFFFFF;
	uint32 DisabledButtonBackgroundColor = 0xFF1F2124;
	uint32 DisabledButtonTextColor = 0xFF757575;

private:
	void SyncSelectionState();
	void UpdateGeometry();

	void SetTranslateMode();
	void SetRotationMode();
	void SetScaleMode();
	void ToggleCoordMode();

	void ApplyButtonStyle(SButton& Button, bool bActive) const;
	bool HandleButtonMouse(SButton& Button, int32 X, int32 Y);

	FRect GetExpandedInteractiveRect() const;
	FEditorViewportClient* GetActiveViewportClient() const;

private:
	FEditorEngine* Engine = nullptr;
	FEditorViewportClient* ViewportClient = nullptr;

	int32 ButtonSize = 24;
	int32 Padding = 4;
	int32 Gap = 4;

	SButton TranslateModeButton;
	SButton RotationModeButton;
	SButton ScaleModeButton;
	SButton ToggleCoordModeButton;
};

