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
	TArray<int32> DesiredWidths;
	DesiredWidths.resize(SlotCount);
	TArray<int32> DesiredHeights;
	DesiredHeights.resize(SlotCount);
	TArray<int32> MinWidths;
	MinWidths.resize(SlotCount);
	TArray<int32> MinHeights;
	MinHeights.resize(SlotCount);
	TArray<int32> AllocatedHeights;
	AllocatedHeights.resize(SlotCount);
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

		const int32 BaseHeight =
			(Slot.HeightFill > 0.0f)
			? MinHeights[Index]
			: (std::max)(DesiredHeights[Index], MinHeights[Index]);

		AxisInputs[Index] = { BaseHeight, MinHeights[Index], Slot.HeightFill };
		TotalPadding += Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom;
	}

	const int32 AvailableHeight =
		(std::max)(0, static_cast<int32>(Rect.Height - TotalPadding));
	AllocatePrimaryAxisSizes(AxisInputs, AvailableHeight, AllocatedHeights);

	int32 CursorY = Rect.Y;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FSlot& Slot = Slots[Index];
		if (!Slot.Widget)
		{
			continue;
		}

		CursorY += static_cast<int32>(Slot.PaddingInsets.Top);
		const int32 AllocatedHeight = (std::max)(0, AllocatedHeights[Index]);
		const int32 AvailableWidth = (std::max)(0, static_cast<int32>(Rect.Width - Slot.PaddingInsets.Left - Slot.PaddingInsets.Right));

		const int32 ChildWidth = ResolveChildExtent(
			AvailableWidth,
			DesiredWidths[Index],
			MinWidths[Index],
			Slot.HAlignment == EHAlign::Fill);

		const int32 ChildHeight = ResolveChildExtent(
			AllocatedHeight,
			DesiredHeights[Index],
			MinHeights[Index],
			Slot.VAlignment == EVAlign::Fill);

		const int32 ChildX = ResolveHorizontalAlignment(AvailableWidth, ChildWidth, Slot.HAlignment, Rect.X + static_cast<int32>(Slot.PaddingInsets.Left));
		const int32 ChildY = ResolveVerticalAlignment(AllocatedHeight, ChildHeight, Slot.VAlignment, CursorY);

		const FRect PaddedSlotRect{
			Rect.X + static_cast<int32>(Slot.PaddingInsets.Left),
			CursorY,
			AvailableWidth,
			AllocatedHeight
		};
		const FRect ChildRect{ ChildX, ChildY, ChildWidth, ChildHeight };
		Slot.Widget->Rect = IntersectRect(ChildRect, PaddedSlotRect);
		Slot.Widget->ArrangeChildren();

		CursorY += AllocatedHeight + static_cast<int32>(Slot.PaddingInsets.Bottom);
	}
}
