#pragma once

#include "Slot.h"

class SPanel : public SWidget
{
public:
	void OnPaint(SWidget& Painter) override
	{
		for (auto& Slot : Slots)
			if (Slot.Widget) Slot.Widget->Paint(Painter);
	}

protected:
	TArray<FSlot> Slots;
};