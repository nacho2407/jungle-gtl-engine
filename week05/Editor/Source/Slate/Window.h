#pragma once

#include "Viewport/ViewportTypes.h"

class SWindow
{
public:
	FRect Rect;
	bool IsHover(FPoint coord) const;
};