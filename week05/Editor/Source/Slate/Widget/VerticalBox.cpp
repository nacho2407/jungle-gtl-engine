#include "VerticalBox.h"
#include <algorithm>

FVector2 SVerticalBox::ComputeDesiredSize() const
{
	float TotalHeight = 0.0f;
	float MaxWidth = 0.0f;

	for (const FSlot& Slot : Slots)
	{
		if (!Slot.Widget)
		{
			continue;
		}

		const FVector2 ChildSize = Slot.Widget->ComputeDesiredSize();
		TotalHeight += ChildSize.Y + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom;
		MaxWidth = (std::max)(MaxWidth, ChildSize.X + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right);
	}
	return { MaxWidth, TotalHeight };
}

FVector2 SVerticalBox::ComputeMinSize() const
{
	float TotalHeight = 0.0f;
	float MaxWidth = 0.0f;

	for (const FSlot& Slot : Slots)
	{
		if (!Slot.Widget)
		{
			continue;
		}

		const FVector2 ChildSize = Slot.Widget->ComputeMinSize();
		TotalHeight += (std::max)(ChildSize.Y, Slot.MinHeight) + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom;
		MaxWidth = (std::max)(MaxWidth, (std::max)(ChildSize.X, Slot.MinWidth) + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right);
	}
	return { MaxWidth, TotalHeight };
}

void SVerticalBox::ArrangeChildren()
{
	if (!Rect.IsValid())
	{
		return;
	}

	const int32 SlotCount = static_cast<int32>(Slots.size());
	TArray<int32> AllocatedHeights;
	AllocatedHeights.resize(SlotCount);
	TArray<int32> MinHeights;
	MinHeights.resize(SlotCount);

	float TotalPadding = 0.0f;
	float TotalBaseHeight = 0.0f;
	float TotalFill = 0.0f;

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FSlot& Slot = Slots[Index];
		if (!Slot.Widget)
		{
			AllocatedHeights[Index] = 0;
			MinHeights[Index] = 0;
			continue;
		}

		const FVector2 DesiredSize = Slot.Widget->ComputeDesiredSize();
		const FVector2 WidgetMinSize = Slot.Widget->ComputeMinSize();
		const int32 DesiredHeight = (std::max)(0, static_cast<int32>(DesiredSize.Y + 0.5f));
		const int32 MinHeight = (std::max)(0, static_cast<int32>((std::max)(WidgetMinSize.Y, Slot.MinHeight) + 0.5f));
		MinHeights[Index] = (std::max)(MinHeight, 0);

		const int32 SlotMinHeight = (std::max)(0, static_cast<int32>(Slot.MinHeight + 0.5f));
		AllocatedHeights[Index] = (Slot.HeightFill > 0.0f) ? MinHeights[Index] : (std::max)(DesiredHeight, SlotMinHeight);
		TotalPadding += Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom;
		TotalBaseHeight += AllocatedHeights[Index];
		if (Slot.HeightFill > 0.0f)
		{
			TotalFill += Slot.HeightFill;
		}
	}

	int32 AvailableHeight = (std::max)(0, static_cast<int32>(Rect.Height - TotalPadding));
	int32 RemainingHeight = AvailableHeight - static_cast<int32>(TotalBaseHeight + 0.5f);

	if (RemainingHeight > 0 && TotalFill > 0.0f)
	{
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			FSlot& Slot = Slots[Index];
			if (!Slot.Widget || Slot.HeightFill <= 0.0f)
			{
				continue;
			}

			const int32 ExtraHeight = static_cast<int32>(RemainingHeight * (Slot.HeightFill / TotalFill) + 0.5f);
			AllocatedHeights[Index] += ExtraHeight;
		}
	}
	else if (RemainingHeight < 0)
	{
		int32 Deficit = -RemainingHeight;
		int32 TotalShrinkable = 0;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			TotalShrinkable += (std::max)(0, AllocatedHeights[Index] - MinHeights[Index]);
		}

		if (TotalShrinkable > 0)
		{
			for (int32 Index = 0; Index < SlotCount; ++Index)
			{
				const int32 Shrinkable = (std::max)(0, AllocatedHeights[Index] - MinHeights[Index]);
				if (Shrinkable <= 0)
				{
					continue;
				}

				const int32 ShrinkAmount = (std::min)(Shrinkable, static_cast<int32>(Deficit * (static_cast<float>(Shrinkable) / TotalShrinkable) + 0.5f));
				AllocatedHeights[Index] -= ShrinkAmount;
			}
		}

		int32 UsedHeight = 0;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			UsedHeight += AllocatedHeights[Index];
		}

		int32 OverBy = UsedHeight - AvailableHeight;
		for (int32 Index = SlotCount - 1; Index >= 0 && OverBy > 0; --Index)
		{
			const int32 Shrinkable = (std::max)(0, AllocatedHeights[Index] - MinHeights[Index]);
			const int32 ShrinkAmount = (std::min)(Shrinkable, OverBy);
			AllocatedHeights[Index] -= ShrinkAmount;
			OverBy -= ShrinkAmount;
		}

		if (OverBy > 0)
		{
			for (int32 Index = SlotCount - 1; Index >= 0 && OverBy > 0; --Index)
			{
				const int32 ShrinkAmount = (std::min)(AllocatedHeights[Index], OverBy);
				AllocatedHeights[Index] -= ShrinkAmount;
				OverBy -= ShrinkAmount;
			}
		}
	}

	int32 CursorY = Rect.Y;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FSlot& Slot = Slots[Index];
		if (!Slot.Widget)
		{
			continue;
		}

		CursorY += static_cast<int32>(Slot.PaddingInsets.Top);
		const FVector2 DesiredSize = Slot.Widget->ComputeDesiredSize();
		const int32 AllocatedHeight = (std::max)(0, AllocatedHeights[Index]);
		const int32 AvailableWidth = (std::max)(0, static_cast<int32>(Rect.Width - Slot.PaddingInsets.Left - Slot.PaddingInsets.Right));
		const int32 SlotMinHeight = (std::max)(0, static_cast<int32>(Slot.MinHeight + 0.5f));
		const int32 SlotMinWidth = (std::max)(0, static_cast<int32>((std::max)(Slot.MinWidth, Slot.Widget->ComputeMinSize().X) + 0.5f));
		const int32 ChildWidth = (Slot.HAlignment == EHAlign::Fill) ? AvailableWidth : (std::max)(SlotMinWidth, (std::min)(AvailableWidth, static_cast<int32>(DesiredSize.X + 0.5f)));
		const int32 ChildHeight = (Slot.VAlignment == EVAlign::Fill) ? AllocatedHeight : (std::max)(SlotMinHeight, (std::min)(AllocatedHeight, static_cast<int32>(DesiredSize.Y + 0.5f)));
		const int32 ChildX = ResolveHorizontalAlignment(AvailableWidth, ChildWidth, Slot.HAlignment, Rect.X + static_cast<int32>(Slot.PaddingInsets.Left));
		const int32 ChildY = ResolveVerticalAlignment(AllocatedHeight, ChildHeight, Slot.VAlignment, CursorY);

		Slot.Widget->Rect = IntersectRect({ ChildX, ChildY, ChildWidth, ChildHeight }, Rect);
		Slot.Widget->ArrangeChildren();

		CursorY += AllocatedHeight + static_cast<int32>(Slot.PaddingInsets.Bottom);
	}
}
