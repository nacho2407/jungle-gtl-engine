#include "Overlay.h"

FVector2 SOverlay::ComputeDesiredSize() const
{
	float MaxHeight = 0.0f;
	float MaxWidth = 0.0f;

	for (auto& Slot : Slots)
	{
		if (!Slot.Widget) continue;
		FVector2 ChildSize = Slot.Widget->ComputeDesiredSize();
		MaxWidth = (std::max)(MaxWidth, ChildSize.X + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right);
		MaxHeight = (std::max)(MaxHeight, ChildSize.Y + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom);
	}

	return { MaxWidth, MaxHeight };
}

void SOverlay::ArrangeChildren()
{
	for (auto& Slot : Slots)
	{
		if (!Slot.Widget) continue;
		Slot.Widget->Rect = {
			Rect.X + (int32)Slot.PaddingInsets.Left,
			Rect.Y + (int32)Slot.PaddingInsets.Top,
			Rect.Width,
			Rect.Height
		};
		Slot.Widget->ArrangeChildren();
	}
}
