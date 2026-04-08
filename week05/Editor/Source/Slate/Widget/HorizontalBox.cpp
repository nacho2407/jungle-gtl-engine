#include "HorizontalBox.h"
#include <algorithm>

FVector2 SHorizontalBox::ComputeDesiredSize() const
{
	float TotalWidth = 0.0f;
	float MaxHeight = 0.0f;

	for (const FSlot& Slot : Slots)
	{
		if (!Slot.Widget)
		{
			continue;
		}

		const FVector2 ChildSize = Slot.Widget->ComputeDesiredSize();
		TotalWidth += ChildSize.X + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right;
		MaxHeight = (std::max)(MaxHeight, ChildSize.Y + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom);
	}

	return { TotalWidth, MaxHeight };
}

FVector2 SHorizontalBox::ComputeMinSize() const
{
	float TotalWidth = 0.0f;
	float MaxHeight = 0.0f;

	for (const FSlot& Slot : Slots)
	{
		if (!Slot.Widget)
		{
			continue;
		}

		const FVector2 ChildSize = Slot.Widget->ComputeMinSize();
		TotalWidth += (std::max)(ChildSize.X, Slot.MinWidth) + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right;
		MaxHeight = (std::max)(MaxHeight, (std::max)(ChildSize.Y, Slot.MinHeight) + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom);
	}

	return { TotalWidth, MaxHeight };
}

void SHorizontalBox::ArrangeChildren()
{
	if (!Rect.IsValid())
	{
		return;
	}

	const int32 SlotCount = static_cast<int32>(Slots.size());
	TArray<int32> AllocatedWidths;
	AllocatedWidths.resize(SlotCount);
	TArray<int32> MinWidths;
	MinWidths.resize(SlotCount);

	float TotalPadding = 0.0f;
	float TotalBaseWidth = 0.0f;
	float TotalFill = 0.0f;

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FSlot& Slot = Slots[Index];
		if (!Slot.Widget)
		{
			AllocatedWidths[Index] = 0;
			MinWidths[Index] = 0;
			continue;
		}

		const FVector2 DesiredSize = Slot.Widget->ComputeDesiredSize();
		const FVector2 WidgetMinSize = Slot.Widget->ComputeMinSize();
		const int32 DesiredWidth = (std::max)(0, static_cast<int32>(DesiredSize.X + 0.5f));
		const int32 MinWidth = (std::max)(0, static_cast<int32>((std::max)(WidgetMinSize.X, Slot.MinWidth) + 0.5f));
		MinWidths[Index] = (std::max)(MinWidth, 0);

		const int32 SlotMinWidth = (std::max)(0, static_cast<int32>(Slot.MinWidth + 0.5f));
		AllocatedWidths[Index] = (Slot.WidthFill > 0.0f) ? MinWidths[Index] : (std::max)(DesiredWidth, SlotMinWidth);
		TotalPadding += Slot.PaddingInsets.Left + Slot.PaddingInsets.Right;
		TotalBaseWidth += AllocatedWidths[Index];
		if (Slot.WidthFill > 0.0f)
		{
			TotalFill += Slot.WidthFill;
		}
	}

	int32 AvailableWidth = (std::max)(0, static_cast<int32>(Rect.Width - TotalPadding));
	int32 RemainingWidth = AvailableWidth - static_cast<int32>(TotalBaseWidth + 0.5f);

	if (RemainingWidth > 0 && TotalFill > 0.0f)
	{
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			FSlot& Slot = Slots[Index];
			if (!Slot.Widget || Slot.WidthFill <= 0.0f)
			{
				continue;
			}

			const int32 ExtraWidth = static_cast<int32>(RemainingWidth * (Slot.WidthFill / TotalFill) + 0.5f);
			AllocatedWidths[Index] += ExtraWidth;
		}
	}
	else if (RemainingWidth < 0)
	{
		int32 Deficit = -RemainingWidth;
		int32 TotalShrinkable = 0;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			TotalShrinkable += (std::max)(0, AllocatedWidths[Index] - MinWidths[Index]);
		}

		if (TotalShrinkable > 0)
		{
			for (int32 Index = 0; Index < SlotCount; ++Index)
			{
				const int32 Shrinkable = (std::max)(0, AllocatedWidths[Index] - MinWidths[Index]);
				if (Shrinkable <= 0)
				{
					continue;
				}

				const int32 ShrinkAmount = (std::min)(Shrinkable, static_cast<int32>(Deficit * (static_cast<float>(Shrinkable) / TotalShrinkable) + 0.5f));
				AllocatedWidths[Index] -= ShrinkAmount;
			}
		}

		int32 UsedWidth = 0;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			UsedWidth += AllocatedWidths[Index];
		}

		int32 OverBy = UsedWidth - AvailableWidth;
		for (int32 Index = SlotCount - 1; Index >= 0 && OverBy > 0; --Index)
		{
			const int32 Shrinkable = (std::max)(0, AllocatedWidths[Index] - MinWidths[Index]);
			const int32 ShrinkAmount = (std::min)(Shrinkable, OverBy);
			AllocatedWidths[Index] -= ShrinkAmount;
			OverBy -= ShrinkAmount;
		}

		if (OverBy > 0)
		{
			for (int32 Index = SlotCount - 1; Index >= 0 && OverBy > 0; --Index)
			{
				const int32 ShrinkAmount = (std::min)(AllocatedWidths[Index], OverBy);
				AllocatedWidths[Index] -= ShrinkAmount;
				OverBy -= ShrinkAmount;
			}
		}
	}

	int32 CursorX = Rect.X;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FSlot& Slot = Slots[Index];
		if (!Slot.Widget)
		{
			continue;
		}

		CursorX += static_cast<int32>(Slot.PaddingInsets.Left);
		const FVector2 DesiredSize = Slot.Widget->ComputeDesiredSize();
		const int32 AllocatedWidth = (std::max)(0, AllocatedWidths[Index]);
		const int32 AvailableHeight = (std::max)(0, static_cast<int32>(Rect.Height - Slot.PaddingInsets.Top - Slot.PaddingInsets.Bottom));
		const int32 SlotMinWidth = (std::max)(0, static_cast<int32>(Slot.MinWidth + 0.5f));
		const int32 ChildWidth = (Slot.HAlignment == EHAlign::Fill) ? AllocatedWidth : (std::max)(SlotMinWidth, (std::min)(AllocatedWidth, static_cast<int32>(DesiredSize.X + 0.5f)));
		const int32 SlotMinHeight = (std::max)(0, static_cast<int32>((std::max)(Slot.MinHeight, Slot.Widget->ComputeMinSize().Y) + 0.5f));
		const int32 ChildHeight = (Slot.VAlignment == EVAlign::Fill) ? AvailableHeight : (std::max)(SlotMinHeight, (std::min)(AvailableHeight, static_cast<int32>(DesiredSize.Y + 0.5f)));
		const int32 ChildX = ResolveHorizontalAlignment(AllocatedWidth, ChildWidth, Slot.HAlignment, CursorX);
		const int32 ChildY = ResolveVerticalAlignment(AvailableHeight, ChildHeight, Slot.VAlignment, Rect.Y + static_cast<int32>(Slot.PaddingInsets.Top));

		Slot.Widget->Rect = IntersectRect({ ChildX, ChildY, ChildWidth, ChildHeight }, Rect);
		Slot.Widget->ArrangeChildren();

		CursorX += AllocatedWidth + static_cast<int32>(Slot.PaddingInsets.Right);
	}
}
