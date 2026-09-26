#pragma once

#include "Panel.h"

class SVerticalBox : public SPanel
{
public:
	bool IsPrimaryAxisVertical() const override { return true; }
	FSlot& AddSlot() { Slots.push_back({}); return Slots.back(); }
	FVector2 ComputeDesiredSize() const override;
	FVector2 ComputeMinSize() const override;
	void ArrangeChildren() override;
};