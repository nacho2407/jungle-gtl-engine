#include "Toolbar.h"

SToolbar::SToolbar()
{
	Rect = { 0, 0, 0, 0 };
}

FRect SToolbar::GetInteractiveRect() const
{
	FRect Expanded = Rect;
	for (const FSlot& Slot : Slots)
	{
		if (!Slot.Widget || !Slot.Widget->Rect.IsValid())
		{
			continue;
		}

		Expanded = UnionRect(Expanded, Slot.Widget->GetPaintClipRect());
	}
	return Expanded;
}

bool SToolbar::HitTest(FPoint Point) const
{
	return ContainsPoint(GetInteractiveRect(), Point);
}
