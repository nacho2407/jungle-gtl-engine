#pragma once

#include "Slot.h"
#include "WidgetHelpers.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

class SPanel : public SWidget
{
public:
	virtual bool IsPrimaryAxisVertical() const { return false; }
	template <typename TWidget, typename... TArgs>
	TWidget& CreateOwnedWidget(TArgs&&... Args)
	{
		return SWidgetHelpers::EmplaceOwned<TArray<std::unique_ptr<SWidget>>, TWidget>(OwnedChildren, std::forward<TArgs>(Args)...);
	}

	STextBlock& AddLabel(const FString& Text, const FMargin& Padding = FMargin(4.0f, 0.0f))
	{
		STextBlock& Label = SWidgetHelpers::MakeLabel(OwnedChildren, Text);
		AddWidget(&Label, 0.0f, Padding, EHAlign::Fill, EVAlign::Center);
		return Label;
	}

	SButton& AddButton(const FString& Label, std::function<void()> OnClick, const FMargin& Padding = FMargin(3.0f, 0.0f))
	{
		SButton& Button = SWidgetHelpers::MakeButton(OwnedChildren, Label, std::move(OnClick));
		AddWidget(&Button, 0.0f, Padding, EHAlign::Fill, EVAlign::Center);
		return Button;
	}

	SButton& AddToggle(
		const FString& Label,
		std::function<bool()> GetValue,
		std::function<void()> OnToggle,
		const FMargin& Padding = FMargin(3.0f, 0.0f))
	{
		SButton& Button = SWidgetHelpers::MakeToggle(OwnedChildren, Label, std::move(GetValue), std::move(OnToggle));
		AddWidget(&Button, 0.0f, Padding, EHAlign::Fill, EVAlign::Center);
		return Button;
	}

	SDropdown& AddDropdown(
		const FString& Label,
		const TArray<FString>& Options,
		std::function<int32()> GetSelectedIndex,
		std::function<void(int32)> OnChanged,
		const FMargin& Padding = FMargin(3.0f, 0.0f),
		ETextVAlign HeaderVAlign = ETextVAlign::Center,
		ETextHAlign OptionHAlign = ETextHAlign::Left,
		ETextVAlign OptionVAlign = ETextVAlign::Center)
	{
		SDropdown& Dropdown = SWidgetHelpers::MakeDropdown(OwnedChildren, Label, Options, std::move(GetSelectedIndex), std::move(OnChanged), HeaderVAlign, OptionHAlign, OptionVAlign);
		AddWidget(&Dropdown, 0.0f, Padding, EHAlign::Fill, EVAlign::Center);
		return Dropdown;
	}

	SSpacer& AddSpacer(float Width = 8.0f)
	{
		SSpacer& Spacer = SWidgetHelpers::MakeSpacer(OwnedChildren, Width);
		AddWidget(&Spacer, 0.0f, FMargin(0.0f), EHAlign::Fill, EVAlign::Fill);
		return Spacer;
	}

	FSlot& AddStretch(float Weight = 1.0f)
	{
		SSpacer& Spacer = SWidgetHelpers::MakeSpacer(OwnedChildren, 1.0f);
		FSlot& Slot = AddWidget(&Spacer, Weight, FMargin(0.0f), EHAlign::Fill, EVAlign::Fill);
		if (IsPrimaryAxisVertical())
		{
			Slot.FillHeight(Weight);
		}
		else
		{
			Slot.FillWidth(Weight);
		}
		Slot.HAlign(EHAlign::Fill).VAlign(EVAlign::Fill);
		return Slot;
	}

	FSlot& AddWidget(
		SWidget* Widget,
		float FillWidth = 0.0f,
		const FMargin& Padding = FMargin(0.0f),
		EHAlign HAlignment = EHAlign::Fill,
		EVAlign VAlignment = EVAlign::Center)
	{
		Slots.push_back({});
		FSlot& Slot = Slots.back();
		Slot[Widget].Padding(Padding).HAlign(HAlignment).VAlign(VAlignment);
		if (FillWidth > 0.0f)
		{
			if (IsPrimaryAxisVertical())
			{
				Slot.FillHeight(FillWidth).VAlign(EVAlign::Fill);
			}
			else
			{
				Slot.FillWidth(FillWidth).HAlign(EHAlign::Fill);
			}
		}
		return Slot;
	}

	TArray<std::unique_ptr<SWidget>>& GetOwnedChildrenStorage() { return OwnedChildren; }
	const TArray<std::unique_ptr<SWidget>>& GetOwnedChildrenStorage() const { return OwnedChildren; }

	void OnPaint(FSlatePaintContext& Painter) override
	{
		for (auto& Slot : Slots)
		{
			if (Slot.Widget && !Slot.Widget->WantsPopupPaintPriority())
			{
				Slot.Widget->Paint(Painter);
			}
		}

		for (auto& Slot : Slots)
		{
			if (Slot.Widget && Slot.Widget->WantsPopupPaintPriority())
			{
				Slot.Widget->Paint(Painter);
			}
		}
	}

	bool OnMouseDown(int32 X, int32 Y) override
	{
		for (int32 Index = static_cast<int32>(Slots.size()) - 1; Index >= 0; --Index)
		{
			FSlot& Slot = Slots[Index];
			if (!Slot.Widget || !Slot.Widget->WantsPopupPaintPriority())
			{
				continue;
			}

			if (Slot.Widget->OnMouseDown(X, Y))
			{
				return true;
			}
		}

		for (int32 Index = static_cast<int32>(Slots.size()) - 1; Index >= 0; --Index)
		{
			FSlot& Slot = Slots[Index];
			if (!Slot.Widget || Slot.Widget->WantsPopupPaintPriority())
			{
				continue;
			}

			if (Slot.Widget->OnMouseDown(X, Y))
			{
				return true;
			}
		}

		return false;
	}

protected:
	static int32 ResolveHorizontalAlignment(int32 AvailableWidth, int32 DesiredWidth, EHAlign Alignment, int32 OriginX)
	{
		switch (Alignment)
		{
		case EHAlign::Left:   return OriginX;
		case EHAlign::Center: return OriginX + (AvailableWidth - DesiredWidth) / 2;
		case EHAlign::Right:  return OriginX + AvailableWidth - DesiredWidth;
		case EHAlign::Fill:   return OriginX;
		}
		return OriginX;
	}

	static int32 ResolveVerticalAlignment(int32 AvailableHeight, int32 DesiredHeight, EVAlign Alignment, int32 OriginY)
	{
		switch (Alignment)
		{
		case EVAlign::Top:    return OriginY;
		case EVAlign::Center: return OriginY + (AvailableHeight - DesiredHeight) / 2;
		case EVAlign::Bottom: return OriginY + AvailableHeight - DesiredHeight;
		case EVAlign::Fill:   return OriginY;
		}
		return OriginY;
	}

	static void SortSlotsByZOrder(TArray<FSlot>& InOutSlots)
	{
		std::stable_sort(InOutSlots.begin(), InOutSlots.end(), [](const FSlot& A, const FSlot& B)
		{
			return A.ZOrder < B.ZOrder;
		});
	}

protected:
	TArray<FSlot> Slots;
	TArray<std::unique_ptr<SWidget>> OwnedChildren;
};

