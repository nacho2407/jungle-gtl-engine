#pragma once

#include "Viewport/ViewportTypes.h"
#include "Widget.h"

class SWindow : public SWidget
{
public:
	virtual EMouseCursor GetCursor() const override { return EMouseCursor::Default; }
};