#pragma once

#include "Panel.h"

constexpr int32 BARWIDTH = 8;

class SSplitter : public SPanel
{
public:
	SSplitter() { Slots.resize(2); }

	float Ratio = 0.0f;
	void SetSideLT(SWidget* InWidget) { Slots[0].Widget = InWidget; }
	void SetSideRB(SWidget* InWidget) { Slots[1].Widget = InWidget; }
	SWidget* GetSideLT() const { return Slots[0].Widget; }
	SWidget* GetSideRB() const { return Slots[1].Widget; }
	uint32 Color = 0xFF3C3C3C;

	virtual void ArrangeChildren() = 0;
	virtual void OnMouseMove(int32 X, int32 Y) = 0;
	virtual FRect GetSplitterBarRect() const = 0;
	virtual EMouseCursor GetCursor() const override { return EMouseCursor::Default; }
	void OnPaint(SWidget& Painter) override { Painter.DrawRectFilled(GetSplitterBarRect(), Color); SPanel::OnPaint(Painter); }
};