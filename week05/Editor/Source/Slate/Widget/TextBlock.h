#pragma once

#include "Widget.h"
#include "TextMetrics.h"
#include <algorithm>

#ifdef DrawText
#undef DrawText
#endif

class STextBlock : public SWidget
{
public:
	~STextBlock() override = default;
	FString Text;
	uint32 Color = 0xFFFFFFFF;
	float FontSize = 48.0f;
	float LetterSpacing = 1.0f;
	ETextHAlign TextHAlign = ETextHAlign::Left;
	ETextVAlign TextVAlign = ETextVAlign::Center;
	
	void SetText(const FString& InText);
	FVector2 ComputeDesiredSize() const override { return SWidgetTextMetrics::MeasureText(Text, FontSize, LetterSpacing); }
	FVector2 ComputeMinSize() const override
	{
		const float MinTextWidth =
			Text.empty() ? 0.0f : SWidgetTextMetrics::MeasureTextWidth("...", FontSize, LetterSpacing);
		const float MinTextHeight =
			SWidgetTextMetrics::MeasureText("Ag", FontSize, LetterSpacing).Y;

		return { MinTextWidth, MinTextHeight };
	}
	void OnPaint(FSlatePaintContext& Painter) override
	{
		if (!Rect.IsValid())
		{
			return;
		}

		auto FitTextToWidth = [&](const FString& SourceText, int32 MaxWidth)
		{
			if (MaxWidth <= 0 || SourceText.empty())
			{
				return FString();
			}

			const float MaxWidthWithTolerance = static_cast<float>(MaxWidth) + 0.75f;
			if (SWidgetTextMetrics::MeasureTextLogicalWidth(SourceText, FontSize, LetterSpacing) <= MaxWidthWithTolerance)
			{
				return SourceText;
			}

			const FString Ellipsis = "...";
			for (size_t PrefixLength = SourceText.size(); PrefixLength > 0;)
			{
				PrefixLength = SWidgetTextMetrics::PrevUtf8PrefixLength(SourceText, PrefixLength);
				const FString Candidate = SourceText.substr(0, PrefixLength) + Ellipsis;
				if (SWidgetTextMetrics::MeasureTextLogicalWidth(Candidate, FontSize, LetterSpacing) <= MaxWidthWithTolerance)
				{
					return Candidate;
				}
			}
			return Ellipsis;
		};

		const FString RenderedText = FitTextToWidth(Text, (std::max)(0, Rect.Width));
		if (CachedRenderedText != RenderedText || CachedLetterSpacing != LetterSpacing)
		{
			CachedRenderedText = RenderedText;
			CachedLetterSpacing = LetterSpacing;
		}

		const FVector2 TextSize = Painter.MeasureText(CachedRenderedText.c_str(), FontSize, LetterSpacing);
		const int32 TextWidth = static_cast<int32>(TextSize.X + 0.5f);
		const int32 TextHeight = static_cast<int32>(TextSize.Y + 0.5f);

		int32 TextX = Rect.X;
		switch (TextHAlign)
		{
		case ETextHAlign::Left:
			TextX = Rect.X;
			break;
		case ETextHAlign::Center:
			TextX = Rect.X + (Rect.Width - TextWidth) / 2;
			break;
		case ETextHAlign::Right:
			TextX = Rect.X + Rect.Width - TextWidth;
			break;
		}

		int32 TextY = Rect.Y;
		switch (TextVAlign)
		{
		case ETextVAlign::Top:
			TextY = Rect.Y;
			break;
		case ETextVAlign::Center:
			TextY = Rect.Y + (Rect.Height - TextHeight) / 2;
			break;
		case ETextVAlign::Bottom:
			TextY = Rect.Y + Rect.Height - TextHeight;
			break;
		}

		Painter.DrawText({ TextX, TextY }, CachedRenderedText.c_str(), Color, FontSize, LetterSpacing);
	}

private:
	FString CachedRenderedText;
	float CachedLetterSpacing = 1.0f;
};
