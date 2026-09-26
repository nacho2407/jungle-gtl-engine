#include "LayoutToolbarWidget.h"
#include "EditorEngine.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Slate/SlateApplication.h"

namespace
{
	const TArray<FString>& GetLayoutOptions()
	{
		static const TArray<FString> Options = {
			"Single",
			"SplitH",
			"SplitV",
			"ThreeLeft",
			"ThreeRight",
			"ThreeTop",
			"ThreeBottom",
			"FourGrid"
		};
		return Options;
	}
}

FVector2 SLayoutToolbarWidget::ComputeDesiredSize() const
{
	const FVector2 Desired = Toolbar.ComputeDesiredSize();
	return { Desired.X + 16.0f, (std::max)(34.0f, Desired.Y + 8.0f) };
}

FVector2 SLayoutToolbarWidget::ComputeMinSize() const
{
	const FVector2 MinSize = Toolbar.ComputeMinSize();
	return { MinSize.X + 16.0f, (std::max)(34.0f, MinSize.Y + 8.0f) };
}

SLayoutToolbarWidget::SLayoutToolbarWidget(FEditorEngine* InEngine, FEditorViewportClient* InViewportClient)
	: Engine(InEngine)
	, TransformWidget(InEngine, InViewportClient)
{
	auto& ViewportLabel = SWidgetHelpers::MakeLabel(Toolbar.GetOwnedChildrenStorage(), "Viewport");

	Toolbar.AddWidget(&ViewportLabel, 0.0f).SetMinWidth(120.0f).HAlign(EHAlign::Left);
	Toolbar.AddStretch(1.0f);

	auto& PlayToggle = Toolbar.AddToggle(
		"Play",
		[this]() -> bool
		{
			return Engine && Engine->IsPIEActive() && !Engine->IsPIEPaused();
		},
		[this]()
		{
			if (Engine)
			{
				if (!Engine->IsPIEActive())
				{
					Engine->StartPIE();
				}
				else if (Engine->IsPIEPaused())
				{
					Engine->TogglePIEPause();
				}
			}
		},
		FMargin(0.0f, 0.0f, 4.0f, 0.0f));
	PlayToggle.bEnabled = (Engine != nullptr);
	PlayToggle.ActiveBackgroundColor = 0xFF2E7D32;
	PlayToggle.InactiveBackgroundColor = 0xFF2C2F33;
	PlayToggle.ActiveBorderColor = 0xFF81C784;
	PlayToggle.InactiveBorderColor = 0xFF5A6068;
	PlayToggle.ActiveTextColor = 0xFFFFFFFF;
	PlayToggle.InactiveTextColor = 0xFFFFFFFF;

	auto& PauseToggle = Toolbar.AddToggle(
		"Pause",
		[this]() -> bool
		{
			return Engine && Engine->IsPIEActive() && Engine->IsPIEPaused();
		},
		[this]()
		{
			if (Engine && Engine->IsPIEActive())
			{
				Engine->TogglePIEPause();
			}
		},
		FMargin(0.0f, 0.0f, 4.0f, 0.0f));
	PauseToggle.bEnabled = (Engine != nullptr);
	PauseToggle.ActiveBackgroundColor = 0xFFF9A825;
	PauseToggle.InactiveBackgroundColor = 0xFF2C2F33;
	PauseToggle.ActiveBorderColor = 0xFFFFF176;
	PauseToggle.InactiveBorderColor = 0xFF5A6068;

	auto& StopToggle = Toolbar.AddToggle(
		"Stop",
		[this]() -> bool
		{
			return Engine && !Engine->IsPIEActive();
		},
		[this]()
		{
			if (Engine && Engine->IsPIEActive())
			{
				Engine->EndPIE();
			}
		},
		FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	StopToggle.bEnabled = (Engine != nullptr);
	StopToggle.ActiveBackgroundColor = 0xFFC62828;
	StopToggle.InactiveBackgroundColor = 0xFF2C2F33;
	StopToggle.ActiveBorderColor = 0xFFEF9A9A;
	StopToggle.InactiveBorderColor = 0xFF5A6068;

	auto& LayoutDropdown = SWidgetHelpers::MakeDropdown(
		Toolbar.GetOwnedChildrenStorage(),
		"Layout",
		GetLayoutOptions(),
		[this]() -> int32
		{
			if (!Engine || !Engine->GetSlateApplication())
			{
				return static_cast<int32>(EViewportLayout::Single);
			}
			return static_cast<int32>(Engine->GetSlateApplication()->GetCurrentLayout());
		},
		[this](int32 SelectedIndex)
		{
			if (!Engine || !Engine->GetSlateApplication())
			{
				return;
			}

			FSlateApplication* Slate = Engine->GetSlateApplication();
			Slate->SetLayout(static_cast<EViewportLayout>(SelectedIndex));
			const int32 ActiveViewportCount = Slate->GetActiveViewportCount();
			int32 EntryIndex = 0;
			for (FViewportEntry& Entry : Engine->GetViewportRegistry().GetEntries())
			{
				Entry.bActive = (EntryIndex < ActiveViewportCount);
				++EntryIndex;
			}
		});

	Toolbar.AddWidget(&LayoutDropdown, 0.0f).Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	Toolbar.AddWidget(&TransformWidget, 0.0f, FMargin(6.0f, 0.0f));
}

void SLayoutToolbarWidget::ArrangeChildren()
{
	if (!Rect.IsValid())
	{
		Toolbar.Rect = { 0, 0, 0, 0 };
		return;
	}

	Toolbar.Rect = { Rect.X + 8, Rect.Y + 4, Rect.Width - 16, Rect.Height - 8 };
	Toolbar.ArrangeChildren();
}

void SLayoutToolbarWidget::OnPaint(FSlatePaintContext& Painter)
{
	if (!Rect.IsValid())
	{
		return;
	}

	Painter.DrawRectFilled(Rect, BackgroundFillColor);
	Painter.DrawRect(Rect, BorderColor);
	Toolbar.Paint(Painter);
}

bool SLayoutToolbarWidget::OnMouseDown(int32 X, int32 Y)
{
	return Toolbar.OnMouseDown(X, Y);
}

bool SLayoutToolbarWidget::HitTest(FPoint Point) const
{
	return Toolbar.HitTest(Point);
}

