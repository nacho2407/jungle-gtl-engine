#pragma once

#include "Widget.h"
#include "TextMetrics.h"
#include <functional>

class SButton : public SWidget
{
public:
	~SButton() override = default;

	FString Text;
	float FontSize = 12.0f;
	float LetterSpacing = 1.0f;
	ETextHAlign TextHAlign = ETextHAlign::Center;
	ETextVAlign TextVAlign = ETextVAlign::Center;
	bool bEnabled = true;

	uint32 BackgroundColor = 0xFF3A3A3A;
	uint32 BorderColor = 0xFF6A6A6A;
	uint32 TextColor = 0xFFFFFFFF;
	uint32 DisabledBackgroundColor = 0xFF2E2E2E;
	uint32 DisabledTextColor = 0xFF9A9A9A;

	std::function<void()> OnClicked;

	FVector2 ComputeDesiredSize() const override
	{
		const FVector2 TextSize = SWidgetTextMetrics::MeasureText(Text, FontSize, LetterSpacing);
		return { TextSize.X + 16.0f, (std::max)(TextSize.Y + 8.0f, FontSize + 12.0f) };
	}
	FVector2 ComputeMinSize() const override
	{
		const float MinTextWidth = SWidgetTextMetrics::MeasureTextWidth("...", FontSize, LetterSpacing);
		const float MinTextHeight = SWidgetTextMetrics::MeasureText("Ag", FontSize, LetterSpacing).Y;
		return { (std::max)(24.0f, MinTextWidth + 16.0f), (std::max)(MinTextHeight + 8.0f, FontSize + 12.0f) };
	}
	void OnPaint(FSlatePaintContext& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;

private:
	FString CachedRenderedText;
	float CachedLetterSpacing = 1.0f;
};

