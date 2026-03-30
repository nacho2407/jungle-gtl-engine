#include "SplitterV.h"
#include "Math/MathUtility.h"

void SSplitterV::OnMouseMove(int32 X, int32 Y)
{
	if (Rect.Height <= 0) return;
	Ratio = FMath::Clamp((float)(Y - Rect.Y) / Rect.Height, 0.1f, 0.9f);
	ArrangeChildren();
}

FRect SSplitterV::GetSplitterBarRect() const
{
	return FRect(Rect.X, Rect.Y + (int32)(Rect.Height * Ratio) - BARWIDTH / 2, Rect.Width, BARWIDTH);
}

void SSplitterV::ArrangeChildren()
{
	SWidget* LT = GetSideLT();
	SWidget* RB = GetSideRB();

	if (LT == nullptr && RB != nullptr)
	{
		RB->Rect = Rect;
		RB->ArrangeChildren();
		return;
	}
	if (LT != nullptr && RB == nullptr)
	{
		LT->Rect = Rect;
		LT->ArrangeChildren();
		return;
	}
	if (LT == nullptr && RB == nullptr)
		return;

	LT->Rect = FRect(Rect.X, Rect.Y, Rect.Width, Rect.Height * Ratio - BARWIDTH / 2);
	LT->ArrangeChildren();
	RB->Rect = FRect(Rect.X, Rect.Y + Rect.Height * Ratio + BARWIDTH / 2, Rect.Width, Rect.Height * (1.0f - Ratio));
	RB->ArrangeChildren();
}
