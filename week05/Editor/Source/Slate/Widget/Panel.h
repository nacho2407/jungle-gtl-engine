#pragma once

#include "Slot.h"
#include "WidgetHelpers.h"
#include <algorithm>
#include <cstdint>
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

	SWidgetHelpers::SToggleButton& AddToggle(
		const FString& Label,
		std::function<bool()> GetValue,
		std::function<void()> OnToggle,
		const FMargin& Padding = FMargin(3.0f, 0.0f))
	{
		SWidgetHelpers::SToggleButton& Button =
			SWidgetHelpers::MakeToggle(OwnedChildren, Label, std::move(GetValue), std::move(OnToggle));

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
		float PrimaryAxisFill = 0.0f,
		const FMargin& Padding = FMargin(0.0f),
		EHAlign HAlignment = EHAlign::Fill,
		EVAlign VAlignment = EVAlign::Center)
	{
		Slots.push_back({});
		FSlot& Slot = Slots.back();
		Slot[Widget].Padding(Padding).HAlign(HAlignment).VAlign(VAlignment);
		if (PrimaryAxisFill > 0.0f)
		{
			if (IsPrimaryAxisVertical())
			{
				Slot.FillHeight(PrimaryAxisFill).VAlign(EVAlign::Fill);
			}
			else
			{
				Slot.FillWidth(PrimaryAxisFill).HAlign(EHAlign::Fill);
			}
		}
		return Slot;
	}

	TArray<std::unique_ptr<SWidget>>& GetOwnedChildrenStorage() { return OwnedChildren; }
	const TArray<std::unique_ptr<SWidget>>& GetOwnedChildrenStorage() const { return OwnedChildren; }

	void OnPaint(FSlatePaintContext& Painter) override
	{
		TArray<int32> NonPopupOrder;
		BuildSlotPaintOrder(Slots, false, NonPopupOrder);
		for (const int32 SlotIndex : NonPopupOrder)
		{
			FSlot& Slot = Slots[SlotIndex];
			Painter.PushLayer(Slot.Layer);
			Painter.PushDepth(static_cast<float>(Slot.ZOrder));
			Slot.Widget->Paint(Painter);
			Painter.PopDepth();
			Painter.PopLayer();
		}

		TArray<int32> PopupOrder;
		BuildSlotPaintOrder(Slots, true, PopupOrder);
		for (const int32 SlotIndex : PopupOrder)
		{
			FSlot& Slot = Slots[SlotIndex];
			Painter.PushLayer(Slot.Layer);
			Painter.PushDepth(static_cast<float>(Slot.ZOrder));
			Slot.Widget->Paint(Painter);
			Painter.PopDepth();
			Painter.PopLayer();
		}
	}

	bool OnMouseDown(int32 X, int32 Y) override
	{
		TArray<int32> PopupOrder;
		BuildSlotPaintOrder(Slots, true, PopupOrder);
		for (int32 Index = static_cast<int32>(PopupOrder.size()) - 1; Index >= 0; --Index)
		{
			FSlot& Slot = Slots[PopupOrder[Index]];
			if (Slot.Widget->OnMouseDown(X, Y))
			{
				return true;
			}
		}

		TArray<int32> NonPopupOrder;
		BuildSlotPaintOrder(Slots, false, NonPopupOrder);
		for (int32 Index = static_cast<int32>(NonPopupOrder.size()) - 1; Index >= 0; --Index)
		{
			FSlot& Slot = Slots[NonPopupOrder[Index]];
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

	static void BuildSlotPaintOrder(const TArray<FSlot>& InSlots, bool bPopupOnly, TArray<int32>& OutOrder)
	{
		OutOrder.clear();
		OutOrder.reserve(InSlots.size());

		const int32 SlotCount = static_cast<int32>(InSlots.size());
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			const FSlot& Slot = InSlots[SlotIndex];
			if (!Slot.Widget || Slot.Widget->WantsPopupPaintPriority() != bPopupOnly)
			{
				continue;
			}

			OutOrder.push_back(SlotIndex);
		}

		std::stable_sort(
			OutOrder.begin(),
			OutOrder.end(),
			[&InSlots](int32 AIndex, int32 BIndex)
			{
				const FSlot& A = InSlots[AIndex];
				const FSlot& B = InSlots[BIndex];
				if (A.Layer != B.Layer)
				{
					return A.Layer < B.Layer;
				}
				if (A.ZOrder != B.ZOrder)
				{
					return A.ZOrder < B.ZOrder;
				}
				return AIndex < BIndex;
			});
	}

	struct FPrimaryAxisAllocation
	{
		int32 BaseSize = 0;
		int32 MinSize = 0;
		float Fill = 0.0f;
	};

	static int32 ResolveChildExtent(int32 AvailableExtent, int32 DesiredExtent, int32 MinExtent, bool bFill)
	{
		const int32 SafeAvailable = (std::max)(0, AvailableExtent);
		const int32 SafeDesired = (std::max)(0, DesiredExtent);
		const int32 SafeMin = (std::max)(0, MinExtent);

		if (bFill)
		{
			return SafeAvailable;
		}

		if (SafeAvailable <= 0)
		{
			return 0;
		}

		if (SafeAvailable <= SafeMin)
		{
			return SafeAvailable;
		}

		return (std::min)(SafeAvailable, (std::max)(SafeMin, SafeDesired));
	}

	static int32 ShrinkSpaceExact(
		TArray<int32>& InOutSizes,
		const TArray<int32>& FloorSizes,
		int32 Deficit)
	{
		const int32 SafeDeficit = (std::max)(0, Deficit);
		if (SafeDeficit <= 0 || InOutSizes.empty() || InOutSizes.size() != FloorSizes.size())
		{
			return 0;
		}

		int32 TotalShrinkable = 0;
		for (size_t Index = 0; Index < InOutSizes.size(); ++Index)
		{
			TotalShrinkable += (std::max)(0, InOutSizes[Index] - FloorSizes[Index]);
		}

		if (TotalShrinkable <= 0)
		{
			return 0;
		}

		const int32 TargetDeficit = (std::min)(SafeDeficit, TotalShrinkable);
		int32 Applied = 0;
		int32 CumulativeShrinkable = 0;

		for (size_t Index = 0; Index < InOutSizes.size(); ++Index)
		{
			const int32 Shrinkable = (std::max)(0, InOutSizes[Index] - FloorSizes[Index]);
			if (Shrinkable <= 0)
			{
				continue;
			}

			CumulativeShrinkable += Shrinkable;
			const int32 TargetApplied = static_cast<int32>((static_cast<int64_t>(TargetDeficit) * CumulativeShrinkable) / TotalShrinkable);
			const int32 Delta = (std::min)(Shrinkable, (std::max)(0, TargetApplied - Applied));
			InOutSizes[Index] -= Delta;
			Applied += Delta;
		}

		return Applied;
	}

	static void AllocatePrimaryAxisSizes(
		const TArray<FPrimaryAxisAllocation>& Inputs,
		int32 AvailableSize,
		TArray<int32>& OutSizes)
	{
		OutSizes.resize(Inputs.size());

		const int32 SafeAvailable = (std::max)(0, AvailableSize);
		int32 TotalBase = 0;
		double TotalFill = 0.0;

		for (size_t Index = 0; Index < Inputs.size(); ++Index)
		{
			OutSizes[Index] = (std::max)(0, Inputs[Index].BaseSize);
			TotalBase += OutSizes[Index];
			if (Inputs[Index].Fill > 0.0f)
			{
				TotalFill += Inputs[Index].Fill;
			}
		}

		if (TotalBase < SafeAvailable && TotalFill > 0.0)
		{
			const int32 Extra = SafeAvailable - TotalBase;
			int32 Assigned = 0;
			double CumulativeFill = 0.0;
			for (size_t Index = 0; Index < Inputs.size(); ++Index)
			{
				if (Inputs[Index].Fill <= 0.0f)
				{
					continue;
				}

				CumulativeFill += Inputs[Index].Fill;
				const double Fraction = CumulativeFill / TotalFill;
				const int32 TargetAssigned = (std::min)(Extra, static_cast<int32>(Extra * Fraction));
				const int32 Delta = (std::max)(0, TargetAssigned - Assigned);
				OutSizes[Index] += Delta;
				Assigned += Delta;
			}
		}
		else if (TotalBase > SafeAvailable)
		{
			int32 Deficit = TotalBase - SafeAvailable;

			TArray<int32> MinFloors;
			MinFloors.resize(Inputs.size());
			for (size_t Index = 0; Index < Inputs.size(); ++Index)
			{
				MinFloors[Index] = (std::min)((std::max)(0, Inputs[Index].MinSize), OutSizes[Index]);
			}

			Deficit -= ShrinkSpaceExact(OutSizes, MinFloors, Deficit);
			if (Deficit > 0)
			{
				TArray<int32> ZeroFloors(Inputs.size(), 0);
				Deficit -= ShrinkSpaceExact(OutSizes, ZeroFloors, Deficit);
				(void)Deficit;
			}
		}
	}

protected:
	TArray<FSlot> Slots;
	TArray<std::unique_ptr<SWidget>> OwnedChildren;
};
