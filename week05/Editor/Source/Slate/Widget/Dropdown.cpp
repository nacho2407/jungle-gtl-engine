#include "Dropdown.h"
#include <algorithm>

SDropdown::~SDropdown()
{
	ClearTextMeshes();
}

void SDropdown::ClearTextMeshes()
{
	for (auto& Pair : TextMeshes)
	{
		delete Pair.second;
	}
	TextMeshes.clear();
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

FVector2 SDropdown::MeasureTextCached(SWidget& Painter, const FString& Text, FDynamicMesh*& OutMesh)
{
	auto It = TextMeshes.find(Text);
	if (It == TextMeshes.end())
	{
		It = TextMeshes.emplace(Text, nullptr).first;
	}

	FDynamicMesh*& CachedMesh = It->second;
	const FVector2 Size = Painter.MeasureText(Text.c_str(), FontSize, LetterSpacing, CachedMesh);
	OutMesh = CachedMesh;
	return Size;
}

void SDropdown::OnPaint(SWidget& Painter)
{
	if (CachedLetterSpacing != LetterSpacing)
	{
		CachedLetterSpacing = LetterSpacing;
		ClearTextMeshes();
	}

	const uint32 BgColor = bEnabled
		? (bOpen ? RowOpenBackgroundColor : RowBackgroundColor)
		: RowDisabledBackgroundColor;

	Painter.DrawRectFilled(Rect, BgColor);
	Painter.DrawRect(Rect, BorderColor);

	const int32 Padding = 8;
	const int32 ArrowPadding = 8;
	const int32 MinLabelWidth = 48;
	const int32 MaxLabelWidth = 96;
	const int32 LabelWidth = (std::clamp)(Rect.Width / 3, MinLabelWidth, MaxLabelWidth);
	const int32 LabelX = Rect.X + Padding;
	const int32 ValueX = LabelX + LabelWidth;
	const FString SelectedText = GetSelectedText();
	const FString ArrowText = bOpen ? "^" : "v";
	FDynamicMesh* LabelMesh = nullptr;
	FDynamicMesh* ValueMesh = nullptr;
	FDynamicMesh* ArrowMesh = nullptr;
	const FVector2 LabelSize = MeasureTextCached(Painter, Label, LabelMesh);
	const FVector2 ValueSize = MeasureTextCached(Painter, SelectedText, ValueMesh);
	const FVector2 ArrowSize = MeasureTextCached(Painter, ArrowText, ArrowMesh);
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

	const int32 LabelY = ComputeHeaderTextY(static_cast<int32>(LabelSize.Y + 0.5f));
	const int32 ValueY = ComputeHeaderTextY(static_cast<int32>(ValueSize.Y + 0.5f));
	const int32 ArrowX = Rect.X + Rect.Width - ArrowPadding - static_cast<int32>(ArrowSize.X + 0.5f);
	const int32 ArrowY = ComputeHeaderTextY(static_cast<int32>(ArrowSize.Y + 0.5f));

	const uint32 LabelColor = bEnabled ? 0xFFE5E5E5 : DisabledTextColor;
	const uint32 ValueColor = bEnabled ? TextColor : DisabledTextColor;

	Painter.DrawText({ LabelX, LabelY }, Label.c_str(), LabelColor, FontSize, LetterSpacing, LabelMesh);
	Painter.DrawText({ ValueX, ValueY }, SelectedText.c_str(), ValueColor, FontSize, LetterSpacing, ValueMesh);
	Painter.DrawText({ ArrowX, ArrowY }, ArrowText.c_str(), LabelColor, FontSize, LetterSpacing, ArrowMesh);

	if (!bOpen)
	{
		return;
	}

	for (int32 OptionIndex = 0; OptionIndex < static_cast<int32>(Options.size()); ++OptionIndex)
	{
		const FRect OptionRect = GetOptionRect(OptionIndex);
		Painter.DrawRectFilled(OptionRect, OptionBackgroundColor);
		Painter.DrawRect(OptionRect, OptionBorderColor);
		FDynamicMesh* OptionMesh = nullptr;
		const FVector2 OptionSize = MeasureTextCached(Painter, Options[OptionIndex], OptionMesh);
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

		Painter.DrawText({ OptionX, OptionY }, Options[OptionIndex].c_str(), TextColor, FontSize, LetterSpacing, OptionMesh);
	}
}

bool SDropdown::OnMouseDown(int32 X, int32 Y)
{
	const bool bInsideHeader =
		(Rect.X <= X && X <= Rect.X + Rect.Width) &&
		(Rect.Y <= Y && Y <= Rect.Y + Rect.Height);

	const FRect Expanded = GetExpandedRect();
	const bool bInsideExpanded =
		Expanded.IsValid() &&
		(Expanded.X <= X && X <= Expanded.X + Expanded.Width) &&
		(Expanded.Y <= Y && Y <= Expanded.Y + Expanded.Height);

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
		const bool bInsideOption =
			(OptionRect.X <= X && X <= OptionRect.X + OptionRect.Width) &&
			(OptionRect.Y <= Y && Y <= OptionRect.Y + OptionRect.Height);

		if (!bInsideOption)
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
