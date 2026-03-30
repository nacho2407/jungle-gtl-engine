#pragma once

#include "Window.h"

class SViewport : public SWindow
{
public:
	FViewportId  Id       = INVALID_VIEWPORT_ID;
	FViewport*   Viewport = nullptr;

	bool HitTest(int32 X, int32 Y) const;
	void OnPaint(SWidget& Painter) override { Painter.DrawRect(Rect, 0xFF282828); }
};