#pragma once

#include "Widget.h"
#include <functional>

class SDropdown : public SWidget
{
public:
	~SDropdown() override = default;

	FString Label = "Dropdown";
	FString Placeholder = "Select";
	float FontSize = 12.0f;
	float LetterSpacing = 1.0f;
	ETextVAlign HeaderTextVAlign = ETextVAlign::Center;
	ETextHAlign OptionTextHAlign = ETextHAlign::Left;
	ETextVAlign OptionTextVAlign = ETextVAlign::Center;
	bool bEnabled = true;

	uint32 RowBackgroundColor = 0xFF3A3A3A;
	uint32 RowOpenBackgroundColor = 0xFF4E5966;
	uint32 RowDisabledBackgroundColor = 0xFF2E2E2E;
	uint32 BorderColor = 0xFF6A6A6A;
	uint32 TextColor = 0xFFFFFFFF;
	uint32 DisabledTextColor = 0xFF9A9A9A;
	uint32 OptionBackgroundColor = 0xFF2C2C2C;
	uint32 OptionBorderColor = 0xFF555555;

	uint32 HeaderLabelColor = 0xFFE5E5E5;
	uint32 HeaderArrowColor = 0xFFE5E5E5;
	uint32 OptionTextColor = 0xFFFFFFFF;
	uint32 DisabledHeaderLabelColor = 0xFF9A9A9A;
	uint32 DisabledHeaderArrowColor = 0xFF9A9A9A;
	uint32 DisabledOptionTextColor = 0xFF9A9A9A;

	void SetOptions(const TArray<FString>& InOptions);
	const TArray<FString>& GetOptions() const { return Options; }

	void SetSelectedIndex(int32 InIndex);
	int32 GetSelectedIndex() const { return SelectedIndex; }

	void SetOpen(bool bInOpen) { bOpen = bInOpen; }
	bool IsOpen() const { return bOpen; }

	int32 GetTotalHeight() const;
	FRect GetExpandedRect() const;

	std::function<void(int32)> OnSelectionChanged;

	FVector2 ComputeDesiredSize() const override;
	FVector2 ComputeMinSize() const override;
	FRect GetPaintClipRect() const override { return GetExpandedRect(); }
	bool WantsPopupPaintPriority() const override { return bOpen; }
	void OnPaint(FSlatePaintContext& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;

private:
	FString GetSelectedText() const;
	FRect GetOptionRect(int32 Index) const;
	FString FitTextToWidth(const FString& Text, int32 MaxWidth);
	float EstimateTextWidth(const FString& Text) const;

private:
	TArray<FString> Options;
	int32 SelectedIndex = -1;
	bool bOpen = false;
};

