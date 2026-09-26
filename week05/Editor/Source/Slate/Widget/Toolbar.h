#pragma once

#include "HorizontalBox.h"

class SToolbar : public SHorizontalBox
{
public:
	SToolbar();

	FRect GetInteractiveRect() const;
	FRect GetPaintClipRect() const override { return GetInteractiveRect(); }
	bool HitTest(FPoint Point) const override;
};
