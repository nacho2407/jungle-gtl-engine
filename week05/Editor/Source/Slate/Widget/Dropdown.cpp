#include "Dropdown.h"
#include "TextMetrics.h"
#include <algorithm>
#include <cmath>

#ifdef DrawText
#undef DrawText
#endif

namespace
{
	constexpr float TextFitTolerancePx = 0.75f;

	static void AllocateHeaderTextWidths(
		int32 AvailableContentWidth,
		int32 Gap,
		int32 DesiredLabelWidth,
		int32 DesiredValueWidth,
		int32 EllipsisWidth,
		int32& OutLabelWidth,
		int32& OutValueWidth)
	{
		OutLabelWidth = 0;
		OutValueWidth = 0;

		if (AvailableContentWidth <= 0)
		{
			return;
		}

		if (DesiredLabelWidth <= 0)
		{
			OutValueWidth = AvailableContentWidth;
			return;
		}

		if (DesiredValueWidth <= 0)
		{
			OutLabelWidth = (std::min)(DesiredLabelWidth, AvailableContentWidth);
			return;
		}

		const int32 AvailableWithoutGap = (std::max)(0, AvailableContentWidth - Gap);
		if (AvailableWithoutGap <= 0)
		{
			OutValueWidth = AvailableContentWidth;
			return;
		}

		if (DesiredLabelWidth + DesiredValueWidth <= AvailableWithoutGap)
		{
			OutLabelWidth = DesiredLabelWidth;
			OutValueWidth = DesiredValueWidth;
			return;
		}

		const int32 MinValueWidth = (std::min)(DesiredValueWidth, (std::max)(EllipsisWidth, 12));
		if (AvailableWithoutGap >= DesiredLabelWidth + MinValueWidth)
		{
			// Label is shown only when its full text fits.
			OutLabelWidth = DesiredLabelWidth;
			OutValueWidth = (std::max)(0, AvailableWithoutGap - DesiredLabelWidth);
			return;
		}

		// If label cannot fit fully, hide it and give space to value.
		OutLabelWidth = 0;
		OutValueWidth = AvailableWithoutGap;
	}
}

void SDropdown::SetOptions(const TArray<FString>& InOptions)
{
	Options = InOptions;
	if (SelectedIndex >= static_cast<int32>(Options.size()))
	{
		SelectedIndex = -1;
	}
}

void SDropdown::SetSelectedIndex(int32 InIndex)
{
	if (InIndex < 0 || InIndex >= static_cast<int32>(Options.size()))
	{
		SelectedIndex = -1;
		return;
	}

	SelectedIndex = InIndex;
}

int32 SDropdown::GetTotalHeight() const
{
	return Rect.Height + (bOpen ? static_cast<int32>(Options.size()) * Rect.Height : 0);
}

FRect SDropdown::GetExpandedRect() const
{
	return { Rect.X, Rect.Y, Rect.Width, GetTotalHeight() };
}

FString SDropdown::GetSelectedText() const
{
	if (SelectedIndex < 0 || SelectedIndex >= static_cast<int32>(Options.size()))
	{
		return Placeholder;
	}

	return Options[SelectedIndex];
}

FRect SDropdown::GetOptionRect(int32 Index) const
{
	return { Rect.X, Rect.Y + Rect.Height * (Index + 1), Rect.Width, Rect.Height };
}

float SDropdown::EstimateTextWidth(const FString& Text) const
{
	return SWidgetTextMetrics::MeasureTextLogicalWidth(Text, FontSize, LetterSpacing);
}

FVector2 SDropdown::ComputeDesiredSize() const
{
	const float Padding = 8.0f;
	const float ArrowWidth = FontSize + 8.0f;
	float MaxTextWidth = (std::max)(EstimateTextWidth(Label), EstimateTextWidth(GetSelectedText()));
	for (const FString& Option : Options)
	{
		MaxTextWidth = (std::max)(MaxTextWidth, EstimateTextWidth(Option));
	}

	const float LabelWidth = EstimateTextWidth(Label);
	const float ValueWidth = (std::max)(MaxTextWidth, EstimateTextWidth(Placeholder));
	const float DesiredWidth = Padding * 4.0f + LabelWidth + ValueWidth + ArrowWidth;
	return { DesiredWidth, FontSize + 12.0f };
}

FVector2 SDropdown::ComputeMinSize() const
{
	const float Padding = 8.0f;
	const float ArrowWidth = FontSize + 8.0f;
	const float MinValueWidth = EstimateTextWidth("...");
	const float MinLabelWidth = Label.empty() ? 0.0f : (std::max)(EstimateTextWidth("..."), 24.0f);
	return { Padding * 4.0f + MinLabelWidth + MinValueWidth + ArrowWidth, FontSize + 12.0f };
}

FString SDropdown::FitTextToWidth(const FString& Text, int32 MaxWidth)
{
	if (MaxWidth <= 0 || Text.empty())
	{
		return "";
	}

	const float MaxWidthWithTolerance = static_cast<float>(MaxWidth) + TextFitTolerancePx;
	if (SWidgetTextMetrics::MeasureTextLogicalWidth(Text, FontSize, LetterSpacing) <= MaxWidthWithTolerance)
	{
		return Text;
	}

	const FString Ellipsis = "...";
	for (size_t PrefixLength = Text.size(); PrefixLength > 0;)
	{
		PrefixLength = SWidgetTextMetrics::PrevUtf8PrefixLength(Text, PrefixLength);
		const FString Candidate = Text.substr(0, PrefixLength) + Ellipsis;
		if (SWidgetTextMetrics::MeasureTextLogicalWidth(Candidate, FontSize, LetterSpacing) <= MaxWidthWithTolerance)
		{
			return Candidate;
		}
	}

	return Ellipsis;
}

void SDropdown::OnPaint(FSlatePaintContext& Painter)
{
	if (!Rect.IsValid())
	{
		return;
	}

	const uint32 BgColor = bEnabled
		? (bOpen ? RowOpenBackgroundColor : RowBackgroundColor)
		: RowDisabledBackgroundColor;

	Painter.DrawRectFilled(Rect, BgColor);
	Painter.DrawRect(Rect, BorderColor);

	const int32 Padding = 8;
	const int32 ArrowPadding = 8;
	const int32 Gap = 8;
	const FString SelectedText = GetSelectedText();
	const FString ArrowText = bOpen ? "^" : "v";

	const FVector2 ArrowSize = Painter.MeasureText(ArrowText.c_str(), FontSize, LetterSpacing);
	const int32 ArrowTextWidth = static_cast<int32>(ArrowSize.X + 0.5f);
	const int32 ArrowX = Rect.X + Rect.Width - ArrowPadding - ArrowTextWidth;
	const int32 ContentLeft = Rect.X + Padding;
	const int32 ContentRight = (std::max)(ContentLeft, ArrowX - Padding);
	const int32 AvailableContentWidth = (std::max)(0, ContentRight - ContentLeft);

	const int32 DesiredLabelWidth = static_cast<int32>(std::ceil(EstimateTextWidth(Label)));
	const int32 DesiredValueWidth = static_cast<int32>(std::ceil(EstimateTextWidth(SelectedText)));
	const int32 EllipsisWidth = static_cast<int32>(std::ceil(EstimateTextWidth("...")));

	int32 LabelWidth = 0;
	int32 ValueWidth = 0;
	const bool bHasLabel = !Label.empty();
	const bool bHasValue = !SelectedText.empty();

	if (!bHasLabel)
	{
		ValueWidth = AvailableContentWidth;
	}
	else if (!bHasValue)
	{
		LabelWidth = AvailableContentWidth;
	}
	else if (AvailableContentWidth > 0)
	{
		AllocateHeaderTextWidths(
			AvailableContentWidth,
			Gap,
			DesiredLabelWidth,
			DesiredValueWidth,
			EllipsisWidth,
			LabelWidth,
			ValueWidth);
	}

	const FString RenderedLabel = FitTextToWidth(Label, LabelWidth);
	const FString RenderedValue = FitTextToWidth(SelectedText, ValueWidth);

	const FVector2 LabelSize = Painter.MeasureText(RenderedLabel.c_str(), FontSize, LetterSpacing);
	const FVector2 ValueSize = Painter.MeasureText(RenderedValue.c_str(), FontSize, LetterSpacing);
	auto ComputeHeaderTextY = [this](int32 TextHeight) -> int32
	{
		switch (HeaderTextVAlign)
		{
		case ETextVAlign::Top:
			return Rect.Y;
		case ETextVAlign::Center:
			return Rect.Y + (Rect.Height - TextHeight) / 2;
		case ETextVAlign::Bottom:
			return Rect.Y + Rect.Height - TextHeight;
		}
		return Rect.Y;
	};

	const int32 LabelX = ContentLeft;
	const int32 ValueX = ContentLeft + LabelWidth + ((LabelWidth > 0 && ValueWidth > 0) ? Gap : 0);
	const int32 LabelY = ComputeHeaderTextY(static_cast<int32>(LabelSize.Y + 0.5f));
	const int32 ValueY = ComputeHeaderTextY(static_cast<int32>(ValueSize.Y + 0.5f));
	const int32 ArrowY = ComputeHeaderTextY(static_cast<int32>(ArrowSize.Y + 0.5f));

	const uint32 LabelColor = bEnabled ? 0xFFE5E5E5 : DisabledTextColor;
	const uint32 ValueColor = bEnabled ? TextColor : DisabledTextColor;

	if (!RenderedLabel.empty())
	{
		Painter.DrawText({ LabelX, LabelY }, RenderedLabel.c_str(), LabelColor, FontSize, LetterSpacing);
	}
	if (!RenderedValue.empty())
	{
		Painter.DrawText({ ValueX, ValueY }, RenderedValue.c_str(), ValueColor, FontSize, LetterSpacing);
	}
	Painter.DrawText({ ArrowX, ArrowY }, ArrowText.c_str(), LabelColor, FontSize, LetterSpacing);

	if (!bOpen)
	{
		return;
	}

	for (int32 OptionIndex = 0; OptionIndex < static_cast<int32>(Options.size()); ++OptionIndex)
	{
		const FRect OptionRect = GetOptionRect(OptionIndex);
		Painter.DrawRectFilled(OptionRect, OptionBackgroundColor);
		Painter.DrawRect(OptionRect, OptionBorderColor);

		const FString RenderedOption = FitTextToWidth(Options[OptionIndex], (std::max)(0, OptionRect.Width - 16));
		const FVector2 OptionSize = Painter.MeasureText(RenderedOption.c_str(), FontSize, LetterSpacing);
		const int32 OptionTextWidth = static_cast<int32>(OptionSize.X + 0.5f);
		const int32 OptionTextHeight = static_cast<int32>(OptionSize.Y + 0.5f);

		int32 OptionX = OptionRect.X + 8;
		switch (OptionTextHAlign)
		{
		case ETextHAlign::Left:
			OptionX = OptionRect.X + 8;
			break;
		case ETextHAlign::Center:
			OptionX = OptionRect.X + (OptionRect.Width - OptionTextWidth) / 2;
			break;
		case ETextHAlign::Right:
			OptionX = OptionRect.X + OptionRect.Width - OptionTextWidth - 8;
			break;
		}

		int32 OptionY = OptionRect.Y;
		switch (OptionTextVAlign)
		{
		case ETextVAlign::Top:
			OptionY = OptionRect.Y;
			break;
		case ETextVAlign::Center:
			OptionY = OptionRect.Y + (OptionRect.Height - OptionTextHeight) / 2;
			break;
		case ETextVAlign::Bottom:
			OptionY = OptionRect.Y + OptionRect.Height - OptionTextHeight;
			break;
		}

		Painter.DrawText({ OptionX, OptionY }, RenderedOption.c_str(), TextColor, FontSize, LetterSpacing);
	}
}

bool SDropdown::OnMouseDown(int32 X, int32 Y)
{
	const bool bInsideHeader = ContainsPoint(Rect, { X, Y });
	const FRect Expanded = GetExpandedRect();
	const bool bInsideExpanded = Expanded.IsValid() && ContainsPoint(Expanded, { X, Y });

	if (!bInsideExpanded)
	{
		bOpen = false;
		return false;
	}

	if (!bEnabled)
	{
		bOpen = false;
		return true;
	}

	if (bInsideHeader)
	{
		bOpen = !bOpen;
		return true;
	}

	if (!bOpen)
	{
		return true;
	}

	for (int32 OptionIndex = 0; OptionIndex < static_cast<int32>(Options.size()); ++OptionIndex)
	{
		const FRect OptionRect = GetOptionRect(OptionIndex);
		if (!ContainsPoint(OptionRect, { X, Y }))
		{
			continue;
		}

		SelectedIndex = OptionIndex;
		bOpen = false;
		if (OnSelectionChanged)
		{
			OnSelectionChanged(OptionIndex);
		}
		return true;
	}

	return true;
}
