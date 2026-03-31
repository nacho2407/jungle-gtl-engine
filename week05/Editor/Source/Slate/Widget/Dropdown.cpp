#include "Dropdown.h"
#include <algorithm>

SDropdown::~SDropdown()
{
	for (auto& Pair : TextMeshes)
	{
		delete Pair.second;
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

void SDropdown::DrawTextCached(SWidget& Painter, const FString& Text, FPoint Position, uint32 Color)
{
	FDynamicMesh*& Mesh = TextMeshes[Text];
	Painter.DrawText(Position, Text.c_str(), Color, FontSize, Mesh);
}

void SDropdown::OnPaint(SWidget& Painter)
{
	const uint32 BgColor = bEnabled
		? (bOpen ? RowOpenBackgroundColor : RowBackgroundColor)
		: RowDisabledBackgroundColor;

	Painter.DrawRectFilled(Rect, BgColor);
	Painter.DrawRect(Rect, BorderColor);

	const int32 Padding = 8;
	const int32 ArrowPadding = 12;
	const int32 MinLabelWidth = 48;
	const int32 MaxLabelWidth = 96;
	const int32 LabelWidth = (std::clamp)(Rect.Width / 3, MinLabelWidth, MaxLabelWidth);
	const int32 LabelX = Rect.X + Padding;
	const int32 ValueX = LabelX + LabelWidth;
	const int32 ArrowX = Rect.X + Rect.Width - ArrowPadding;
	const int32 TextY = Rect.Y + (Rect.Height - static_cast<int32>(FontSize)) / 2;

	const uint32 LabelColor = bEnabled ? 0xFFE5E5E5 : DisabledTextColor;
	const uint32 ValueColor = bEnabled ? TextColor : DisabledTextColor;

	DrawTextCached(Painter, Label, { LabelX, TextY }, LabelColor);
	DrawTextCached(Painter, GetSelectedText(), { ValueX, TextY }, ValueColor);
	DrawTextCached(Painter, bOpen ? "^" : "v", { ArrowX, TextY }, LabelColor);

	if (!bOpen)
	{
		return;
	}

	for (int32 OptionIndex = 0; OptionIndex < static_cast<int32>(Options.size()); ++OptionIndex)
	{
		const FRect OptionRect = GetOptionRect(OptionIndex);
		Painter.DrawRectFilled(OptionRect, OptionBackgroundColor);
		Painter.DrawRect(OptionRect, OptionBorderColor);
		DrawTextCached(Painter, Options[OptionIndex], { OptionRect.X + 8, OptionRect.Y + (OptionRect.Height - static_cast<int32>(FontSize)) / 2 }, TextColor);
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
