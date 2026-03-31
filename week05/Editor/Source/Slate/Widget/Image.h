#pragma once

#include "Widget.h"

class SImage : public SWidget
{
public:
	uint32 TintColor = 0xFFFFFFFF;

	void OnPaint(SWidget& Painter) override { Painter.DrawRectFilled(Rect, TintColor); }
};