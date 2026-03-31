#pragma once

#include "Panel.h"

class SOverlay : public SPanel
{
public:
	FSlot& AddSlot() { Slots.push_back({}); return Slots.back(); }

	FVector2 ComputeDesiredSize() const override;
	void ArrangeChildren() override;
};