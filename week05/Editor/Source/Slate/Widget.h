#pragma once

#include "Viewport/ViewportTypes.h"

class SWidget
{
public:
	virtual ~SWidget() {}

	FRect Rect;

	void Paint(SWidget& Painter) { Onpaint(Painter); }
	virtual void Onpaint(SWidget& Painter) {};
	virtual FVector2 ComputeDesiredSize() const { return { 0, 0 }; }
	virtual bool IsHover(FPoint Point) const;

	virtual void DrawRectFilled(FRect Rect, uint32 Color) {}
	virtual void DrawRect(FRect Rect, uint32 Color) {}
	virtual void DrawText(FPoint Point, const char* Text, uint32 Color) {}
};