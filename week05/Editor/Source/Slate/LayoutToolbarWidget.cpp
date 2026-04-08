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

	Painter.DrawRectFilled(Rect, 0xD01C1E21);
	Painter.DrawRect(Rect, 0xFF555B63);
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

