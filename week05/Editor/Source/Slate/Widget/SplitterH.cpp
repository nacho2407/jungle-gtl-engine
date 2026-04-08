#include "SplitterH.h"
#include <algorithm>

void SSplitterH::OnMouseMove(int32 X, int32 Y)
{
	(void)Y;

	const SWidget* LT = GetSideLT();
	const SWidget* RB = GetSideRB();
	if (!LT || !RB)
	{
		return;
	}

	const int32 AvailableWidth = (std::max)(0, Rect.Width - BARWIDTH);
	if (AvailableWidth <= 0)
	{
		return;
	}

	const int32 LTMinWidth = static_cast<int32>(LT->ComputeMinSize().X + 0.5f);
	const int32 RBMinWidth = static_cast<int32>(RB->ComputeMinSize().X + 0.5f);

	const float RawRatio = static_cast<float>(X - Rect.X) / static_cast<float>(AvailableWidth);
	Ratio = ClampRatioByMinExtents(RawRatio, AvailableWidth, LTMinWidth, RBMinWidth);
	ArrangeChildren();
}

FRect SSplitterH::GetSplitterBarRect() const
{
	const SWidget* LT = GetSideLT();
	const SWidget* RB = GetSideRB();
	if (!LT || !RB || !Rect.IsValid())
	{
		return { 0, 0, 0, 0 };
	}

	const int32 AvailableWidth = (std::max)(0, Rect.Width - BARWIDTH);
	const int32 LTMinWidth = static_cast<int32>(LT->ComputeMinSize().X + 0.5f);
	const int32 RBMinWidth = static_cast<int32>(RB->ComputeMinSize().X + 0.5f);
	const float EffectiveRatio = ClampRatioByMinExtents(Ratio, AvailableWidth, LTMinWidth, RBMinWidth);
	const int32 LTWidth = (std::max)(0, static_cast<int32>(AvailableWidth * EffectiveRatio + 0.5f));
	return FRect(Rect.X + LTWidth, Rect.Y, BARWIDTH, Rect.Height);
}

FVector2 SSplitterH::ComputeDesiredSize() const
{
	const SWidget* LT = GetSideLT();
	const SWidget* RB = GetSideRB();

	const FVector2 LTSize = LT ? LT->ComputeDesiredSize() : FVector2{ 0.0f, 0.0f };
	const FVector2 RBSize = RB ? RB->ComputeDesiredSize() : FVector2{ 0.0f, 0.0f };
	const float Bar = (LT && RB) ? static_cast<float>(BARWIDTH) : 0.0f;

	return { LTSize.X + RBSize.X + Bar, (std::max)(LTSize.Y, RBSize.Y) };
}

FVector2 SSplitterH::ComputeMinSize() const
{
	const SWidget* LT = GetSideLT();
	const SWidget* RB = GetSideRB();

	const FVector2 LTSize = LT ? LT->ComputeMinSize() : FVector2{ 0.0f, 0.0f };
	const FVector2 RBSize = RB ? RB->ComputeMinSize() : FVector2{ 0.0f, 0.0f };
	const float Bar = (LT && RB) ? static_cast<float>(BARWIDTH) : 0.0f;

	return { LTSize.X + RBSize.X + Bar, (std::max)(LTSize.Y, RBSize.Y) };
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
	if ((LT == nullptr && RB == nullptr) || !Rect.IsValid())
	{
		return;
	}

	const int32 AvailableWidth = (std::max)(0, Rect.Width - BARWIDTH);
	const int32 LTMinWidth = static_cast<int32>(LT->ComputeMinSize().X + 0.5f);
	const int32 RBMinWidth = static_cast<int32>(RB->ComputeMinSize().X + 0.5f);

	Ratio = ClampRatioByMinExtents(Ratio, AvailableWidth, LTMinWidth, RBMinWidth);

	const int32 LTWidth = (std::max)(0, static_cast<int32>(AvailableWidth * Ratio + 0.5f));
	const int32 RBWidth = (std::max)(0, AvailableWidth - LTWidth);

	LT->Rect = FRect(Rect.X, Rect.Y, LTWidth, Rect.Height);
	LT->ArrangeChildren();

	RB->Rect = FRect(Rect.X + LTWidth + BARWIDTH, Rect.Y, RBWidth, Rect.Height);
	RB->ArrangeChildren();
}
