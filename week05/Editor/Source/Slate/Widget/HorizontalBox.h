#pragma once

#include "Panel.h"

class SHorizontalBox : public SPanel
{
public:
	FSlot& AddSlot() { Slots.push_back({}); return Slots.back(); }
	FVector2 ComputeDesiredSize() const override;
	FVector2 ComputeMinSize() const override;
	void ArrangeChildren() override;
};