#pragma once

#include "Window.h"

class SViewport : public SWindow
{
public:
	FViewportId  Id       = INVALID_VIEWPORT_ID;
	FViewport*   Viewport = nullptr;

	uint32 OutlineColor = 0xFF282828;

	bool HitTest(int32 X, int32 Y) const;
	void OnPaint(FSlatePaintContext& Painter) override
	{
		Painter.DrawRect(Rect, OutlineColor);
	}
};
