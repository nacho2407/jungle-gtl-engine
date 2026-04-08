#pragma once

#include "Button.h"
#include "Dropdown.h"
#include "TextBlock.h"
#include "Spacer.h"
#include <functional>
#include <memory>
#include <utility>

namespace SWidgetHelpers
{
	class SToggleButton : public SButton
	{
	public:
		std::function<bool()> GetValue;
		std::function<void()> OnToggle;

		uint32 ActiveBackgroundColor = 0xFF3B5E84;
		uint32 InactiveBackgroundColor = 0xFF2C2F33;
		uint32 ActiveBorderColor = 0xFF86C8FF;
		uint32 InactiveBorderColor = 0xFF5A6068;
		uint32 ActiveTextColor = 0xFFFFFFFF;
		uint32 InactiveTextColor = 0xFFFFFFFF;
		uint32 ActiveDisabledBackgroundColor = 0xFF1F2124;
		uint32 InactiveDisabledBackgroundColor = 0xFF1F2124;
		uint32 ActiveDisabledTextColor = 0xFF757575;
		uint32 InactiveDisabledTextColor = 0xFF757575;

		void OnPaint(FSlatePaintContext& Painter) override
		{
			const bool bActive = GetValue ? GetValue() : false;

			BackgroundColor = bActive ? ActiveBackgroundColor : InactiveBackgroundColor;
			BorderColor = bActive ? ActiveBorderColor : InactiveBorderColor;
			TextColor = bActive ? ActiveTextColor : InactiveTextColor;
			DisabledBackgroundColor = bActive ? ActiveDisabledBackgroundColor : InactiveDisabledBackgroundColor;
			DisabledTextColor = bActive ? ActiveDisabledTextColor : InactiveDisabledTextColor;

			SButton::OnPaint(Painter);
		}

		bool OnMouseDown(int32 X, int32 Y) override
		{
			if (!IsHover({ X, Y }))
			{
				return false;
			}

			if (bEnabled && OnToggle)
			{
				OnToggle();
			}
			return true;
		}
	};

	class SBoundDropdown : public SDropdown
	{
	public:
		std::function<int32()> GetSelectedIndex;

		void OnPaint(FSlatePaintContext& Painter) override
		{
			if (GetSelectedIndex)
			{
				SetSelectedIndex(GetSelectedIndex());
			}
			SDropdown::OnPaint(Painter);
		}
	};

	template <typename TOwnedList, typename TWidget, typename... TArgs>
	TWidget& EmplaceOwned(TOwnedList& OwnedChildren, TArgs&&... Args)
	{
		auto Widget = std::make_unique<TWidget>(std::forward<TArgs>(Args)...);
		TWidget* Raw = Widget.get();
		OwnedChildren.push_back(std::move(Widget));
		return *Raw;
	}

	inline void ApplyDefaultLabelStyle(STextBlock& Label)
	{
		Label.FontSize = 18.0f;
		Label.LetterSpacing = 0.5f;
		Label.TextVAlign = ETextVAlign::Center;
	}

	inline void ApplyDefaultButtonStyle(SButton& Button)
	{
		Button.FontSize = 18.0f;
		Button.LetterSpacing = 0.5f;
		Button.BackgroundColor = 0xFF2C2F33;
		Button.BorderColor = 0xFF5A6068;
		Button.TextColor = 0xFFFFFFFF;
		Button.DisabledBackgroundColor = 0xFF1F2124;
		Button.DisabledTextColor = 0xFF757575;
	}

	inline void ApplyDefaultDropdownStyle(SDropdown& Dropdown)
	{
		Dropdown.Placeholder = "";
		Dropdown.FontSize = 18.0f;
		Dropdown.LetterSpacing = 0.5f;

		Dropdown.RowBackgroundColor = 0xFF2C2F33;
		Dropdown.RowOpenBackgroundColor = 0xFF3B5E84;
		Dropdown.RowDisabledBackgroundColor = 0xFF1F2124;
		Dropdown.BorderColor = 0xFF5A6068;
		Dropdown.TextColor = 0xFFFFFFFF;
		Dropdown.DisabledTextColor = 0xFF757575;
		Dropdown.OptionBackgroundColor = 0xFF2C2F33;
		Dropdown.OptionBorderColor = 0xFF5A6068;

		Dropdown.HeaderLabelColor = 0xFFE5E5E5;
		Dropdown.HeaderArrowColor = 0xFFE5E5E5;
		Dropdown.OptionTextColor = 0xFFFFFFFF;
		Dropdown.DisabledHeaderLabelColor = 0xFF757575;
		Dropdown.DisabledHeaderArrowColor = 0xFF757575;
		Dropdown.DisabledOptionTextColor = 0xFF757575;
	}

	template <typename TOwnedList>
	STextBlock& MakeLabel(TOwnedList& OwnedChildren, const FString& Text)
	{
		STextBlock& Label = EmplaceOwned<TOwnedList, STextBlock>(OwnedChildren);
		Label.SetText(Text);
		ApplyDefaultLabelStyle(Label);
		return Label;
	}

	template <typename TOwnedList>
	SButton& MakeButton(TOwnedList& OwnedChildren, const FString& Label, std::function<void()> OnClick)
	{
		SButton& Button = EmplaceOwned<TOwnedList, SButton>(OwnedChildren);
		Button.Text = Label;
		Button.OnClicked = std::move(OnClick);
		ApplyDefaultButtonStyle(Button);
		return Button;
	}

	template <typename TOwnedList>
	SToggleButton& MakeToggle(TOwnedList& OwnedChildren, const FString& Label, std::function<bool()> GetValue, std::function<void()> OnToggle)
	{
		SToggleButton& Button = EmplaceOwned<TOwnedList, SToggleButton>(OwnedChildren);
		Button.Text = Label;
		Button.GetValue = std::move(GetValue);
		Button.OnToggle = std::move(OnToggle);
		ApplyDefaultButtonStyle(Button);
		return Button;
	}

	template <typename TOwnedList>
	SDropdown& MakeDropdown(
		TOwnedList& OwnedChildren,
		const FString& Label,
		const TArray<FString>& Options,
		std::function<int32()> GetSelectedIndex,
		std::function<void(int32)> OnChanged,
		ETextVAlign HeaderVAlign = ETextVAlign::Center,
		ETextHAlign OptionHAlign = ETextHAlign::Left,
		ETextVAlign OptionVAlign = ETextVAlign::Center)
	{
		SBoundDropdown& Dropdown = EmplaceOwned<TOwnedList, SBoundDropdown>(OwnedChildren);
		Dropdown.Label = Label;
		Dropdown.SetOptions(Options);
		Dropdown.GetSelectedIndex = std::move(GetSelectedIndex);
		Dropdown.OnSelectionChanged = std::move(OnChanged);
		Dropdown.HeaderTextVAlign = HeaderVAlign;
		Dropdown.OptionTextHAlign = OptionHAlign;
		Dropdown.OptionTextVAlign = OptionVAlign;
		ApplyDefaultDropdownStyle(Dropdown);
		return Dropdown;
	}

	template <typename TOwnedList>
	SSpacer& MakeSpacer(TOwnedList& OwnedChildren, float Width = 8.0f)
	{
		return EmplaceOwned<TOwnedList, SSpacer>(OwnedChildren, FVector2{ Width, 1.0f });
	}
}

