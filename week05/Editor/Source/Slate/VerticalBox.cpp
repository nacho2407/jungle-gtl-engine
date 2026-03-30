#include "VerticalBox.h"

FVector2 SVerticalBox::ComputeDesiredSize() const
{
	float TotalHeight = 0.0f;
	float MaxWidth = 0.0f;

	for (auto& Slot : Slots)
	{
		if (!Slot.Widget) continue;
		FVector2 ChildSize = Slot.Widget->ComputeDesiredSize();
		TotalHeight += ChildSize.Y + Slot.PaddingInsets.Top + Slot.PaddingInsets.Bottom;
		MaxWidth = (std::max)(MaxWidth, ChildSize.X + Slot.PaddingInsets.Left + Slot.PaddingInsets.Right);
	}
	return { MaxWidth, TotalHeight };
}

void SVerticalBox::ArrangeChildren()
{
	int32 Y = Rect.Y;

	for (auto& Slot : Slots)
	{
		if (!Slot.Widget) continue;
		int32 H = (Slot.HeightFill > 0.0f) ? (int32)(Rect.Height * Slot.HeightFill) : (int32)Slot.Widget->ComputeDesiredSize().Y;
		Slot.Widget->Rect = { Rect.X, Y, Rect.Width, H };
		Slot.Widget->ArrangeChildren();
		Y += H;
	}
}
