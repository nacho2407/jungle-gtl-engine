#include "Button.h"

#include <algorithm>

#ifdef DrawText
#undef DrawText
#endif

namespace
{
	constexpr float TextFitTolerancePx = 0.75f;

	static FString FitTextToWidth(const FString& Text, float FontSize, float LetterSpacing, int32 MaxWidth)
	{
		if (MaxWidth <= 0 || Text.empty())
		{
			return "";
		}

		const float FullWidth = SWidgetTextMetrics::MeasureTextLogicalWidth(Text, FontSize, LetterSpacing);
		const float MaxWidthWithTolerance = static_cast<float>(MaxWidth) + TextFitTolerancePx;
		if (FullWidth <= MaxWidthWithTolerance)
		{
			return Text;
		}

		const FString Ellipsis = "...";
		for (size_t PrefixLength = Text.size(); PrefixLength > 0;)
		{
			PrefixLength = SWidgetTextMetrics::PrevUtf8PrefixLength(Text, PrefixLength);
			const FString Candidate = Text.substr(0, PrefixLength) + Ellipsis;
			const float CandidateWidth = SWidgetTextMetrics::MeasureTextLogicalWidth(Candidate, FontSize, LetterSpacing);
			if (CandidateWidth <= MaxWidthWithTolerance)
			{
				return Candidate;
			}
		}

		return Ellipsis;
	}
}

void SButton::OnPaint(FSlatePaintContext& Painter)
{
	if (!Rect.IsValid())
	{
		return;
	}

	const uint32 BgColor = bEnabled ? BackgroundColor : DisabledBackgroundColor;
	const uint32 LabelColor = bEnabled ? TextColor : DisabledTextColor;

	Painter.DrawRectFilled(Rect, BgColor);
	Painter.DrawRect(Rect, BorderColor);

	const int32 MaxTextWidth = (std::max)(0, Rect.Width - 8);
	const FString RenderedText = FitTextToWidth(Text, FontSize, LetterSpacing, MaxTextWidth);
	if (CachedRenderedText != RenderedText || CachedLetterSpacing != LetterSpacing)
	{
		CachedRenderedText = RenderedText;
		CachedLetterSpacing = LetterSpacing;
	}

	const FVector2 TextSize = Painter.MeasureText(CachedRenderedText.c_str(), FontSize, LetterSpacing);
	const int32 RoundedWidth = static_cast<int32>(TextSize.X + 0.5f);
	const int32 RoundedHeight = static_cast<int32>(TextSize.Y + 0.5f);

	int32 TextX = Rect.X;
	switch (TextHAlign)
	{
	case ETextHAlign::Left:   TextX = Rect.X + 4; break;
	case ETextHAlign::Center: TextX = Rect.X + (Rect.Width - RoundedWidth) / 2; break;
	case ETextHAlign::Right:  TextX = Rect.X + Rect.Width - RoundedWidth - 4; break;
	}

	int32 TextY = Rect.Y;
	switch (TextVAlign)
	{
	case ETextVAlign::Top:    TextY = Rect.Y; break;
	case ETextVAlign::Center: TextY = Rect.Y + (Rect.Height - RoundedHeight) / 2; break;
	case ETextVAlign::Bottom: TextY = Rect.Y + Rect.Height - RoundedHeight; break;
	}

	Painter.DrawText({ TextX, TextY }, CachedRenderedText.c_str(), LabelColor, FontSize, LetterSpacing);
}

bool SButton::OnMouseDown(int32 X, int32 Y)
{
	if (!IsHover({ X, Y }))
	{
		return false;
	}

	if (bEnabled && OnClicked)
	{
		OnClicked();
	}

	return true;
}

