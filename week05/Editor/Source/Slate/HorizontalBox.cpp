#include "HorizontalBox.h"

FVector2 SHorizontalBox::ComputeDesiredSize() const
{
	float TotalWidth = 0.0f;
	float MaxHeight = 0.0f;

	for (auto& Slot : Slots)
	{
		if (!Slot.Widget) continue;
		FVector2 ChildSize = Slot.Widget->ComputeDesiredSize();
		TotalWidth += ChildSize.X + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right;
		MaxHeight = (std::max)(MaxHeight, ChildSize.Y + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom);
	}
	return { TotalWidth, MaxHeight };
}

void SHorizontalBox::ArrangeChildren()
{
	int32 X = Rect.X;

	for (auto& Slot : Slots)
	{
		if (!Slot.Widget) continue;
		int32 W = (Slot.WidthFill > 0.0f) ? (int32)(Rect.Width * Slot.WidthFill) : (int32)Slot.Widget->ComputeDesiredSize().X;
		Slot.Widget->Rect = { X, Rect.Y, W, Rect.Height };
		Slot.Widget->ArrangeChildren();
		X += W;
	}
}
