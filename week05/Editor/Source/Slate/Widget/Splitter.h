#pragma once

#include "Panel.h"
#include <algorithm>

constexpr int32 BARWIDTH = 8;

class SSplitter : public SPanel
{
public:
	SSplitter() { Slots.resize(2); }

	float Ratio = 0.0f;
	void SetSideLT(SWidget* InWidget) { Slots[0].Widget = InWidget; }
	void SetSideRB(SWidget* InWidget) { Slots[1].Widget = InWidget; }
	SWidget* GetSideLT() const { return Slots[0].Widget; }
	SWidget* GetSideRB() const { return Slots[1].Widget; }
	uint32 Color = 0xFF3C3C3C;

	virtual void ArrangeChildren() = 0;
	virtual void OnMouseMove(int32 X, int32 Y) = 0;
	virtual FRect GetSplitterBarRect() const = 0;
	virtual EMouseCursor GetCursor() const override { return EMouseCursor::Default; }
	void OnPaint(FSlatePaintContext& Painter) override
	{
		if (GetSideLT() && GetSideRB())
		{
			const FRect BarRect = GetSplitterBarRect();
			if (BarRect.IsValid())
			{
				Painter.DrawRectFilled(BarRect, Color);
			}
		}
		SPanel::OnPaint(Painter);
	}

protected:
	static float ClampRatioByMinExtents(
		float InRatio,
		int32 TotalPrimaryExtent,
		int32 LTMinExtent,
		int32 RBMinExtent)
	{
		if (TotalPrimaryExtent <= 0)
		{
			return 0.5f;
		}

		const int32 SafeTotal = (std::max)(0, TotalPrimaryExtent);
		const int32 SafeLTMin = (std::max)(0, LTMinExtent);
		const int32 SafeRBMin = (std::max)(0, RBMinExtent);

		const float MinRatio = static_cast<float>((std::min)(SafeLTMin, SafeTotal)) / static_cast<float>(SafeTotal);
		const float MaxRatio = 1.0f - static_cast<float>((std::min)(SafeRBMin, SafeTotal)) / static_cast<float>(SafeTotal);

		if (MinRatio > MaxRatio)
		{
			const float Denom = static_cast<float>((std::max)(1, SafeLTMin + SafeRBMin));
			return static_cast<float>(SafeLTMin) / Denom;
		}

		return (std::max)(MinRatio, (std::min)(InRatio, MaxRatio));
	}
};
