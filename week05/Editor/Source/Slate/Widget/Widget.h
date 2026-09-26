#pragma once

#include "Viewport/ViewportTypes.h"
#include "Painter.h"

#ifdef DrawText
#undef DrawText
#endif

enum class EMouseCursor
{
	Default,
	None,
	TextEdit,
	ResizeLeftRight,
	ResizeUpDown,
	Hand,
};

enum class ETextHAlign
{
	Left,
	Center,
	Right
};

enum class ETextVAlign
{
	Top,
	Center,
	Bottom
};

class SWidget
{
public:
	virtual ~SWidget() {}

	FRect Rect;

	void Paint(FSlatePaintContext& Painter)
	{
		if (ShouldClipToBounds())
		{
			const FRect ClipRect = GetPaintClipRect();
			if (ClipRect.IsValid())
			{
				Painter.PushClipRect(ClipRect);
				OnPaint(Painter);
				Painter.PopClipRect();
				return;
			}
		}

		OnPaint(Painter);
	}

	virtual void OnPaint(FSlatePaintContext& Painter) {}
	virtual FVector2 ComputeDesiredSize() const { return { 0, 0 }; }
	virtual FVector2 ComputeMinSize() const { return { 0, 0 }; }
	virtual void ArrangeChildren() {}
	virtual bool IsHover(FPoint Point) const;
	virtual bool HitTest(FPoint Point) const { return IsHover(Point); }

	virtual bool OnMouseDown(int32 X, int32 Y) { (void)X; (void)Y; return false; }
	virtual FRect GetPaintClipRect() const { return Rect; }
	virtual bool ShouldClipToBounds() const { return Rect.IsValid(); }
	virtual bool WantsPopupPaintPriority() const { return false; }

	virtual EMouseCursor GetCursor() const { return EMouseCursor::Default; }
	virtual bool IsChromeProvider() const { return false; }
	virtual SWidget* GetGlobalChromeWidget() { return nullptr; }
	virtual SWidget* GetViewportChromeWidget(FViewportId ViewportId) { (void)ViewportId; return nullptr; }
};

