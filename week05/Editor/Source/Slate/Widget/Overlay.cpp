#include "Overlay.h"
#include <algorithm>

FVector2 SOverlay::ComputeDesiredSize() const
{
	float MaxHeight = 0.0f;
	float MaxWidth = 0.0f;

	for (const FSlot& Slot : Slots)
	{
		if (!Slot.Widget)
		{
			continue;
		}

		const FVector2 ChildSize = Slot.Widget->ComputeDesiredSize();
		MaxWidth = (std::max)(MaxWidth, ChildSize.X + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right);
		MaxHeight = (std::max)(MaxHeight, ChildSize.Y + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom);
	}

	return { MaxWidth, MaxHeight };
}

FVector2 SOverlay::ComputeMinSize() const
{
	float MaxHeight = 0.0f;
	float MaxWidth = 0.0f;

	for (const FSlot& Slot : Slots)
	{
		if (!Slot.Widget)
		{
			continue;
		}

		const FVector2 ChildSize = Slot.Widget->ComputeMinSize();
		MaxWidth = (std::max)(MaxWidth, (std::max)(ChildSize.X, Slot.MinWidth) + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right);
		MaxHeight = (std::max)(MaxHeight, (std::max)(ChildSize.Y, Slot.MinHeight) + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom);
	}

	return { MaxWidth, MaxHeight };
}

void SOverlay::ArrangeChildren()
{
	SortSlotsByZOrder(Slots);

	for (FSlot& Slot : Slots)
	{
		if (!Slot.Widget)
		{
			continue;
		}

		const FVector2 DesiredSize = Slot.Widget->ComputeDesiredSize();
		const int32 AvailableWidth = (std::max)(0, static_cast<int32>(Rect.Width - Slot.PaddingInsets.Left - Slot.PaddingInsets.Right));
		const int32 AvailableHeight = (std::max)(0, static_cast<int32>(Rect.Height - Slot.PaddingInsets.Top - Slot.PaddingInsets.Bottom));
		const int32 SlotMinWidth = (std::max)(0, static_cast<int32>((std::max)(Slot.MinWidth, Slot.Widget->ComputeMinSize().X) + 0.5f));
		const int32 SlotMinHeight = (std::max)(0, static_cast<int32>((std::max)(Slot.MinHeight, Slot.Widget->ComputeMinSize().Y) + 0.5f));
		const int32 ChildWidth = (Slot.HAlignment == EHAlign::Fill) ? AvailableWidth : (std::max)(SlotMinWidth, (std::min)(AvailableWidth, static_cast<int32>(DesiredSize.X + 0.5f)));
		const int32 ChildHeight = (Slot.VAlignment == EVAlign::Fill) ? AvailableHeight : (std::max)(SlotMinHeight, (std::min)(AvailableHeight, static_cast<int32>(DesiredSize.Y + 0.5f)));
		const int32 ChildX = ResolveHorizontalAlignment(AvailableWidth, ChildWidth, Slot.HAlignment, Rect.X + static_cast<int32>(Slot.PaddingInsets.Left));
		const int32 ChildY = ResolveVerticalAlignment(AvailableHeight, ChildHeight, Slot.VAlignment, Rect.Y + static_cast<int32>(Slot.PaddingInsets.Top));

		Slot.Widget->Rect = IntersectRect({ ChildX, ChildY, ChildWidth, ChildHeight }, Rect);
		Slot.Widget->ArrangeChildren();
	}
}
