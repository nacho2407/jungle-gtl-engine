#include "SplitterH.h"
#include "Math/MathUtility.h"

void SSplitterH::OnMouseMove(int32 X, int32 Y)
{
	if (Rect.Width <= 0) return;
	Ratio = FMath::Clamp((float)(X - Rect.X) / Rect.Width, 0.1f, 0.9f);
	ArrangeChildren();
}

FRect SSplitterH::GetSplitterBarRect() const
{
	return FRect(Rect.X + (int32)(Rect.Width * Ratio) - BARWIDTH / 2, Rect.Y, BARWIDTH, Rect.Height);
}

void SSplitterH::ArrangeChildren()
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

	LT->Rect = FRect(Rect.X, Rect.Y, Rect.Width * Ratio - BARWIDTH / 2, Rect.Height);
	LT->ArrangeChildren();
	RB->Rect = FRect(Rect.X + Rect.Width * Ratio + BARWIDTH / 2, Rect.Y, Rect.Width * (1.0f - Ratio), Rect.Height);
	RB->ArrangeChildren();
}
