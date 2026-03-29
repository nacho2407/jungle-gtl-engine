#pragma once

#include "Viewport/ViewportTypes.h"

class SWindow
{
public:
	virtual ~SWindow() {}
	FRect Rect;
	bool IsHover(FPoint coord) const;
};