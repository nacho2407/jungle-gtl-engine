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
	TArray<int32> DesiredWidths;
	DesiredWidths.resize(SlotCount);
	TArray<int32> DesiredHeights;
	DesiredHeights.resize(SlotCount);
	TArray<int32> MinWidths;
	MinWidths.resize(SlotCount);
	TArray<int32> MinHeights;
	MinHeights.resize(SlotCount);
	TArray<int32> AllocatedWidths;
	AllocatedWidths.resize(SlotCount);
	TArray<FPrimaryAxisAllocation> AxisInputs;
	AxisInputs.resize(SlotCount);

	float TotalPadding = 0.0f;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FSlot& Slot = Slots[Index];
		if (!Slot.Widget)
		{
			DesiredWidths[Index] = 0;
			DesiredHeights[Index] = 0;
			MinWidths[Index] = 0;
			MinHeights[Index] = 0;
			AxisInputs[Index] = {};
			continue;
		}

		const FVector2 DesiredSize = Slot.Widget->ComputeDesiredSize();
		const FVector2 WidgetMinSize = Slot.Widget->ComputeMinSize();

		DesiredWidths[Index] = (std::max)(0, static_cast<int32>(DesiredSize.X + 0.5f));
		DesiredHeights[Index] = (std::max)(0, static_cast<int32>(DesiredSize.Y + 0.5f));
		MinWidths[Index] = (std::max)(0, static_cast<int32>((std::max)(WidgetMinSize.X, Slot.MinWidth) + 0.5f));
		MinHeights[Index] = (std::max)(0, static_cast<int32>((std::max)(WidgetMinSize.Y, Slot.MinHeight) + 0.5f));

		const int32 BaseWidth =
			(Slot.WidthFill > 0.0f)
			? MinWidths[Index]
			: (std::max)(DesiredWidths[Index], MinWidths[Index]);

		AxisInputs[Index] = { BaseWidth, MinWidths[Index], Slot.WidthFill };
		TotalPadding += Slot.PaddingInsets.Left + Slot.PaddingInsets.Right;
	}

	const int32 AvailableWidth =
		(std::max)(0, static_cast<int32>(Rect.Width - TotalPadding));
	AllocatePrimaryAxisSizes(AxisInputs, AvailableWidth, AllocatedWidths);

	int32 CursorX = Rect.X;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FSlot& Slot = Slots[Index];
		if (!Slot.Widget)
		{
			continue;
		}

		CursorX += static_cast<int32>(Slot.PaddingInsets.Left);
		const int32 AllocatedWidth = (std::max)(0, AllocatedWidths[Index]);
		const int32 AvailableHeight = (std::max)(0, static_cast<int32>(Rect.Height - Slot.PaddingInsets.Top - Slot.PaddingInsets.Bottom));

		const int32 ChildWidth = ResolveChildExtent(
			AllocatedWidth,
			DesiredWidths[Index],
			MinWidths[Index],
			Slot.HAlignment == EHAlign::Fill);

		const int32 ChildHeight = ResolveChildExtent(
			AvailableHeight,
			DesiredHeights[Index],
			MinHeights[Index],
			Slot.VAlignment == EVAlign::Fill);

		const int32 ChildX = ResolveHorizontalAlignment(AllocatedWidth, ChildWidth, Slot.HAlignment, CursorX);
		const int32 ChildY = ResolveVerticalAlignment(AvailableHeight, ChildHeight, Slot.VAlignment, Rect.Y + static_cast<int32>(Slot.PaddingInsets.Top));

		const FRect PaddedSlotRect{ CursorX, Rect.Y + static_cast<int32>(Slot.PaddingInsets.Top), AllocatedWidth, AvailableHeight };
		const FRect ChildRect{ ChildX, ChildY, ChildWidth, ChildHeight };
		Slot.Widget->Rect = IntersectRect(ChildRect, PaddedSlotRect);
		Slot.Widget->ArrangeChildren();

		CursorX += AllocatedWidth + static_cast<int32>(Slot.PaddingInsets.Right);
	}
}
