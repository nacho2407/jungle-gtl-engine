#pragma once

#include "Widget/Widget.h"
#include "ViewportToolbar.h"
#include "TransformWidget.h"
#include "FpsStatWidget.h"

class FEditorEngine;
class FEditorUI;
class FEditorViewportClient;

class SEditorViewportOverlay : public SWidget
{
public:
	SEditorViewportOverlay(FEditorEngine* InEngine, FEditorUI* InEditorUI, FEditorViewportClient* InViewportClient);

	void OnPaint(SWidget& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;
	bool HitTest(FPoint Point) const override;

private:
	void UpdateLayout() const;
	bool ComputeViewportBounds(FRect& OutRect) const;
	bool ShouldShowFPS() const;
	FRect GetInteractiveRect() const;

	static bool ContainsPoint(const FRect& InRect, FPoint Point);
	static FRect UnionRects(const FRect& A, const FRect& B);

private:
	FEditorEngine* Engine = nullptr;
	FEditorUI* EditorUI = nullptr;
	mutable FRect ViewportBounds;
	mutable SViewportToolbarWidget Toolbar;
	mutable FTransformWidget Transform;
	mutable FpsStatWidget FpsWidget;
};
