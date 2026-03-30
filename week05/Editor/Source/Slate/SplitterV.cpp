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

	SideLT->Rect = FRect(Rect.X, Rect.Y, Rect.Width, Rect.Height * Ratio - BARWIDTH / 2);
	SideRB->Rect = FRect(Rect.X, Rect.Y + Rect.Height * Ratio + BARWIDTH / 2, Rect.Width, Rect.Height * (1.0f - Ratio));

	if (SSplitter* S = dynamic_cast<SSplitter*>(SideLT)) S->ArrangeChildren();
	if (SSplitter* S = dynamic_cast<SSplitter*>(SideRB)) S->ArrangeChildren();
}
