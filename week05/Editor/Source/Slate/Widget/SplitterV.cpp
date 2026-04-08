#include "SplitterV.h"
#include <algorithm>

void SSplitterV::OnMouseMove(int32 X, int32 Y)
{
	(void)X;

	const SWidget* LT = GetSideLT();
	const SWidget* RB = GetSideRB();
	if (!LT || !RB)
	{
		return;
	}

	const int32 AvailableHeight = (std::max)(0, Rect.Height - BARWIDTH);
	if (AvailableHeight <= 0)
	{
		return;
	}

	const int32 LTMinHeight = static_cast<int32>(LT->ComputeMinSize().Y + 0.5f);
	const int32 RBMinHeight = static_cast<int32>(RB->ComputeMinSize().Y + 0.5f);

	const float RawRatio = static_cast<float>(Y - Rect.Y) / static_cast<float>(AvailableHeight);
	Ratio = ClampRatioByMinExtents(RawRatio, AvailableHeight, LTMinHeight, RBMinHeight);
	ArrangeChildren();
}

FRect SSplitterV::GetSplitterBarRect() const
{
	const SWidget* LT = GetSideLT();
	const SWidget* RB = GetSideRB();
	if (!LT || !RB || !Rect.IsValid())
	{
		return { 0, 0, 0, 0 };
	}

	const int32 AvailableHeight = (std::max)(0, Rect.Height - BARWIDTH);
	const int32 LTMinHeight = static_cast<int32>(LT->ComputeMinSize().Y + 0.5f);
	const int32 RBMinHeight = static_cast<int32>(RB->ComputeMinSize().Y + 0.5f);
	const float EffectiveRatio = ClampRatioByMinExtents(Ratio, AvailableHeight, LTMinHeight, RBMinHeight);
	const int32 LTHeight = (std::max)(0, static_cast<int32>(AvailableHeight * EffectiveRatio + 0.5f));
	return FRect(Rect.X, Rect.Y + LTHeight, Rect.Width, BARWIDTH);
}

FVector2 SSplitterV::ComputeDesiredSize() const
{
	const SWidget* LT = GetSideLT();
	const SWidget* RB = GetSideRB();

	const FVector2 LTSize = LT ? LT->ComputeDesiredSize() : FVector2{ 0.0f, 0.0f };
	const FVector2 RBSize = RB ? RB->ComputeDesiredSize() : FVector2{ 0.0f, 0.0f };
	const float Bar = (LT && RB) ? static_cast<float>(BARWIDTH) : 0.0f;

	return { (std::max)(LTSize.X, RBSize.X), LTSize.Y + RBSize.Y + Bar };
}

FVector2 SSplitterV::ComputeMinSize() const
{
	const SWidget* LT = GetSideLT();
	const SWidget* RB = GetSideRB();

	const FVector2 LTSize = LT ? LT->ComputeMinSize() : FVector2{ 0.0f, 0.0f };
	const FVector2 RBSize = RB ? RB->ComputeMinSize() : FVector2{ 0.0f, 0.0f };
	const float Bar = (LT && RB) ? static_cast<float>(BARWIDTH) : 0.0f;

	return { (std::max)(LTSize.X, RBSize.X), LTSize.Y + RBSize.Y + Bar };
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
	if ((LT == nullptr && RB == nullptr) || !Rect.IsValid())
	{
		return;
	}

	const int32 AvailableHeight = (std::max)(0, Rect.Height - BARWIDTH);
	const int32 LTMinHeight = static_cast<int32>(LT->ComputeMinSize().Y + 0.5f);
	const int32 RBMinHeight = static_cast<int32>(RB->ComputeMinSize().Y + 0.5f);

	Ratio = ClampRatioByMinExtents(Ratio, AvailableHeight, LTMinHeight, RBMinHeight);

	const int32 LTHeight = (std::max)(0, static_cast<int32>(AvailableHeight * Ratio + 0.5f));
	const int32 RBHeight = (std::max)(0, AvailableHeight - LTHeight);

	LT->Rect = FRect(Rect.X, Rect.Y, Rect.Width, LTHeight);
	LT->ArrangeChildren();

	RB->Rect = FRect(Rect.X, Rect.Y + LTHeight + BARWIDTH, Rect.Width, RBHeight);
	RB->ArrangeChildren();
}
