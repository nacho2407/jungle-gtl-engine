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
	if (SideLT == nullptr && SideRB != nullptr)
	{
		SideRB->Rect = Rect;
		return;
	}
	if (SideLT != nullptr && SideRB == nullptr)
	{
		SideLT->Rect = Rect;
		return;
	}
	if (SideLT == nullptr && SideRB == nullptr)
		return;

	SideLT->Rect = FRect(Rect.X, Rect.Y, Rect.Width * Ratio - BARWIDTH / 2, Rect.Height);
	SideRB->Rect = FRect(Rect.X + Rect.Width * Ratio + BARWIDTH / 2, Rect.Y, Rect.Width * (1.0f - Ratio), Rect.Height);

	if (SSplitter* S = dynamic_cast<SSplitter*>(SideLT)) S->ArrangeChildren();
	if (SSplitter* S = dynamic_cast<SSplitter*>(SideRB)) S->ArrangeChildren();
}
