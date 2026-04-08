#include "ViewportToolbar.h"

#include "Actor/Actor.h"
#include "EditorEngine.h"
#include "World/WorldContext.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Slate/SlateApplication.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr const char* PIECaptureHintText = "Shift + F1 for Mouse Cursor";
	constexpr float PIECaptureHintFontSize = 20.0f;
	constexpr float PIECaptureHintLetterSpacing = 0.5f;

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

	const TArray<FString>& GetViewportTypeOptions()
	{
		static const TArray<FString> Options = {
			"Perspective",
			"Top",
			"Bottom",
			"Left",
			"Right",
			"Front",
			"Back"
		};
		return Options;
	}

	const TArray<FString>& GetRenderModeOptions()
	{
		static const TArray<FString> Options = {
			"Lighting",
			"NoLighting",
			"Wireframe"
		};
		return Options;
	}
}

SViewportToolbarWidget::SViewportToolbarWidget(FEditorEngine* InEngine)
	: Engine(InEngine)
{
	Rect = { 0, 0, 330, 0 };
	TitleButton.Text = "View";
	TitleButton.bEnabled = false;
	TitleButton.FontSize = 18.0f;
	TitleButton.LetterSpacing = 0.5f;
	TitleButton.DisabledBackgroundColor = 0xFF1F1F1F;
	TitleButton.BorderColor = 0xFF4E4E4E;
	TitleButton.DisabledTextColor = 0xFFBDBDBD;
	TitleButton.TextHAlign = ETextHAlign::Center;

	LayoutDropdown.Label = "Layout";
	LayoutDropdown.Placeholder = "Select";
	LayoutDropdown.FontSize = 18.0f;
	LayoutDropdown.LetterSpacing = 0.5f;
	LayoutDropdown.SetOptions(GetLayoutOptions());
	LayoutDropdown.OnSelectionChanged = [this](int32 SelectedIndex)
	{
		ApplyLayout(static_cast<EViewportLayout>(SelectedIndex));
	};

	TypeDropdown.Label = "Type";
	TypeDropdown.Placeholder = "";
	TypeDropdown.FontSize = 18.0f;
	TypeDropdown.LetterSpacing = 0.5f;
	TypeDropdown.SetOptions(GetViewportTypeOptions());
	TypeDropdown.OnSelectionChanged = [this](int32 SelectedIndex)
	{
		ApplyViewportType(static_cast<EViewportType>(SelectedIndex));
	};

	ModeDropdown.Label = "Mode";
	ModeDropdown.Placeholder = "";
	ModeDropdown.FontSize = 18.0f;
	ModeDropdown.LetterSpacing = 0.5f;
	ModeDropdown.SetOptions(GetRenderModeOptions());
	ModeDropdown.OnSelectionChanged = [this](int32 SelectedIndex)
	{
		ApplyRenderMode(static_cast<ERenderMode>(SelectedIndex));
	};
}

void SViewportToolbarWidget::ConfigureForGlobalLayout()
{
	bShowLayout = true;
	bShowViewportSettings = false;
	TargetViewportId = INVALID_VIEWPORT_ID;
	TitleButton.Text = "Viewport";
	CloseAllDropdowns();
}

void SViewportToolbarWidget::ConfigureForViewport(FViewportId InViewportId)
{
	bShowLayout = false;
	bShowViewportSettings = true;
	TargetViewportId = InViewportId;
	TitleButton.Text = "View";
	CloseAllDropdowns();
}

FVector2 SViewportToolbarWidget::ComputeDesiredSize() const
{
	float Width = static_cast<float>(EstimateTitleWidth());
	if (bShowLayout)
	{
		Width += LayoutDropdown.ComputeDesiredSize().X + 6.0f;
	}
	if (bShowViewportSettings)
	{
		Width += TypeDropdown.ComputeDesiredSize().X + ModeDropdown.ComputeDesiredSize().X + 12.0f;
		if (ShouldShowPIECaptureHint())
		{
			Width += SWidgetTextMetrics::MeasureTextWidth(PIECaptureHintText, PIECaptureHintFontSize, PIECaptureHintLetterSpacing) + 8.0f;
		}
	}
	return { Width, 24.0f };
}

FVector2 SViewportToolbarWidget::ComputeMinSize() const
{
	float Width = 48.0f;
	if (bShowLayout)
	{
		Width += LayoutDropdown.ComputeMinSize().X + 6.0f;
	}
	if (bShowViewportSettings)
	{
		Width += TypeDropdown.ComputeMinSize().X + ModeDropdown.ComputeMinSize().X + 12.0f;
		if (ShouldShowPIECaptureHint())
		{
			Width += SWidgetTextMetrics::MeasureTextWidth("Shift + F1", PIECaptureHintFontSize, PIECaptureHintLetterSpacing) + 8.0f;
		}
	}
	return { Width, 24.0f };
}

void SViewportToolbarWidget::OnPaint(FSlatePaintContext& Painter)
{
	SyncSelectionState();
	UpdateGeometry();

	if (!Rect.IsValid())
	{
		return;
	}

	TitleButton.Paint(Painter);
	if (bShowLayout)
	{
		LayoutDropdown.Paint(Painter);
	}
	if (bShowViewportSettings)
	{
		if (!TypeDropdown.IsOpen())
		{
			TypeDropdown.Paint(Painter);
		}
		if (!ModeDropdown.IsOpen())
		{
			ModeDropdown.Paint(Painter);
		}
		if (TypeDropdown.IsOpen())
		{
			TypeDropdown.Paint(Painter);
		}
		if (ModeDropdown.IsOpen())
		{
			ModeDropdown.Paint(Painter);
		}
		if (PIECaptureHintRect.IsValid() && ShouldShowPIECaptureHint())
		{
			const FVector2 TextSize = Painter.MeasureText(PIECaptureHintText, PIECaptureHintFontSize, PIECaptureHintLetterSpacing);
			const int32 TextY = PIECaptureHintRect.Y + (std::max)(0, static_cast<int32>((PIECaptureHintRect.Height - TextSize.Y) * 0.5f));
			Painter.PushClipRect(PIECaptureHintRect);
			Painter.DrawText({ PIECaptureHintRect.X, TextY }, PIECaptureHintText, 0xFFD8D8D8, PIECaptureHintFontSize, PIECaptureHintLetterSpacing);
			Painter.PopClipRect();
		}
	}
}

bool SViewportToolbarWidget::OnMouseDown(int32 X, int32 Y)
{
	SyncSelectionState();
	UpdateGeometry();

	if (!HitTest({ X, Y }))
	{
		CloseAllDropdowns();
		UpdateGeometry();
		return false;
	}

	if (bShowLayout && HandleDropdownMouse(LayoutDropdown, EDropdownId::Layout, X, Y))
	{
		return true;
	}
	if (bShowViewportSettings && HandleDropdownMouse(TypeDropdown, EDropdownId::Type, X, Y))
	{
		return true;
	}
	if (bShowViewportSettings && HandleDropdownMouse(ModeDropdown, EDropdownId::Mode, X, Y))
	{
		return true;
	}

	CloseAllDropdowns();
	UpdateGeometry();
	return true;
}

bool SViewportToolbarWidget::HitTest(FPoint Point) const
{
	return ContainsPoint(GetInteractiveRect(), Point);
}

void SViewportToolbarWidget::SetHeaderRect(const FRect& InRect)
{
	Rect = InRect;
	UpdateGeometry();
}

FRect SViewportToolbarWidget::GetInteractiveRect() const
{
	FRect Expanded = Rect;
	if (bShowLayout && LayoutDropdown.IsOpen())
	{
		Expanded = UnionRect(Expanded, LayoutDropdown.GetExpandedRect());
	}
	if (bShowViewportSettings && TypeDropdown.IsOpen())
	{
		Expanded = UnionRect(Expanded, TypeDropdown.GetExpandedRect());
	}
	if (bShowViewportSettings && ModeDropdown.IsOpen())
	{
		Expanded = UnionRect(Expanded, ModeDropdown.GetExpandedRect());
	}
	return Expanded;
}

bool SViewportToolbarWidget::HasOpenDropdown() const
{
	return (bShowLayout && LayoutDropdown.IsOpen()) || (bShowViewportSettings && (TypeDropdown.IsOpen() || ModeDropdown.IsOpen()));
}

void SViewportToolbarWidget::SyncSelectionState()
{
	LayoutDropdown.bEnabled = bShowLayout;
	if (bShowLayout)
	{
		LayoutDropdown.SetSelectedIndex(static_cast<int32>(GetCurrentLayout()));
	}
	else
	{
		LayoutDropdown.SetOpen(false);
	}

	FViewportEntry* TargetEntry = GetTargetEntry();
	const bool bHasTargetEntry = (TargetEntry != nullptr) && bShowViewportSettings;

	TypeDropdown.bEnabled = bHasTargetEntry;
	ModeDropdown.bEnabled = bHasTargetEntry;

	if (!bHasTargetEntry)
	{
		TypeDropdown.SetSelectedIndex(-1);
		ModeDropdown.SetSelectedIndex(-1);
		TypeDropdown.SetOpen(false);
		ModeDropdown.SetOpen(false);
		return;
	}

	TypeDropdown.SetSelectedIndex(static_cast<int32>(TargetEntry->LocalState.ProjectionType));
	ModeDropdown.SetSelectedIndex(static_cast<int32>(TargetEntry->LocalState.ViewMode));
}

int32 SViewportToolbarWidget::EstimateTitleWidth() const
{
	return static_cast<int32>(std::ceil(TitleButton.ComputeDesiredSize().X));
}

void SViewportToolbarWidget::UpdateGeometry()
{
	if (!Rect.IsValid())
	{
		TitleButton.Rect = { 0, 0, 0, 0 };
		LayoutDropdown.Rect = { 0, 0, 0, 0 };
		TypeDropdown.Rect = { 0, 0, 0, 0 };
		ModeDropdown.Rect = { 0, 0, 0, 0 };
		PIECaptureHintRect = { 0, 0, 0, 0 };
		return;
	}

	const int32 Gap = 6;
	const int32 RowHeight = Rect.Height;
	const int32 RowY = Rect.Y;
	int32 CursorX = Rect.X;

	const int32 DesiredTitleWidth = EstimateTitleWidth();
	const int32 MinTitleWidth = 44;
	const int32 LayoutDesiredWidth = static_cast<int32>(std::ceil(LayoutDropdown.ComputeDesiredSize().X));
	const int32 LayoutMinWidth = static_cast<int32>(std::ceil(LayoutDropdown.ComputeMinSize().X));
	const int32 TypeDesiredWidth = static_cast<int32>(std::ceil(TypeDropdown.ComputeDesiredSize().X));
	const int32 TypeMinWidth = static_cast<int32>(std::ceil(TypeDropdown.ComputeMinSize().X));
	const int32 ModeDesiredWidth = static_cast<int32>(std::ceil(ModeDropdown.ComputeDesiredSize().X));
	const int32 ModeMinWidth = static_cast<int32>(std::ceil(ModeDropdown.ComputeMinSize().X));
	const bool bShowPIECaptureHint = ShouldShowPIECaptureHint();
	const int32 HintDesiredWidth = bShowPIECaptureHint
		? static_cast<int32>(std::ceil(SWidgetTextMetrics::MeasureTextWidth(PIECaptureHintText, PIECaptureHintFontSize, PIECaptureHintLetterSpacing)))
		: 0;
	const int32 HintGap = bShowPIECaptureHint ? 8 : 0;

	if (bShowLayout)
	{
		int32 TitleWidth = DesiredTitleWidth;
		int32 LayoutWidth = LayoutDesiredWidth;
		const int32 DesiredTotal = TitleWidth + Gap + LayoutWidth;
		if (DesiredTotal > Rect.Width)
		{
			const int32 Deficit = DesiredTotal - Rect.Width;
			const int32 TitleShrink = (std::min)(TitleWidth - MinTitleWidth, Deficit / 2);
			TitleWidth -= (std::max)(0, TitleShrink);
			LayoutWidth = (std::max)(LayoutMinWidth, Rect.Width - TitleWidth - Gap);
		}

		TitleButton.Rect = IntersectRect({ CursorX, RowY, TitleWidth, RowHeight }, Rect);
		CursorX += TitleWidth + Gap;
		LayoutDropdown.Rect = IntersectRect({ CursorX, RowY, (std::max)(0, LayoutWidth), RowHeight }, Rect);
		TypeDropdown.Rect = { 0, 0, 0, 0 };
		ModeDropdown.Rect = { 0, 0, 0, 0 };
		PIECaptureHintRect = { 0, 0, 0, 0 };
		return;
	}

		int32 TitleWidth = DesiredTitleWidth;
		int32 TypeWidth = TypeDesiredWidth;
		int32 ModeWidth = ModeDesiredWidth;
		const int32 DesiredTotal = TitleWidth + Gap + TypeWidth + Gap + ModeWidth + HintGap + HintDesiredWidth;
		if (DesiredTotal > Rect.Width)
		{
			TitleWidth = (std::min)(DesiredTitleWidth, (std::max)(MinTitleWidth, Rect.Width / 4));
			const int32 WidthForDropdowns = (std::max)(0, Rect.Width - TitleWidth - Gap * 2 - HintGap - HintDesiredWidth);
			const int32 TotalDesired = TypeDesiredWidth + ModeDesiredWidth;
			if (TotalDesired > 0)
			{
				TypeWidth = (std::max)(TypeMinWidth, static_cast<int32>(WidthForDropdowns * (static_cast<float>(TypeDesiredWidth) / TotalDesired) + 0.5f));
				ModeWidth = (std::max)(ModeMinWidth, WidthForDropdowns - TypeWidth);
			}
			if (TypeWidth + ModeWidth > WidthForDropdowns)
			{
				const int32 Over = TypeWidth + ModeWidth - WidthForDropdowns;
				const int32 ShrinkMode = (std::min)(Over, ModeWidth - ModeMinWidth);
				ModeWidth -= (std::max)(0, ShrinkMode);
				TypeWidth = (std::max)(TypeMinWidth, WidthForDropdowns - ModeWidth);
			}
		}

		TitleButton.Rect = IntersectRect({ CursorX, RowY, TitleWidth, RowHeight }, Rect);
		CursorX += TitleWidth + Gap;
		TypeDropdown.Rect = IntersectRect({ CursorX, RowY, (std::max)(0, TypeWidth), RowHeight }, Rect);
		CursorX += TypeWidth + Gap;
		ModeDropdown.Rect = IntersectRect({ CursorX, RowY, (std::max)(0, ModeWidth), RowHeight }, Rect);
		CursorX += ModeWidth + HintGap;
		PIECaptureHintRect = bShowPIECaptureHint
			? IntersectRect({ CursorX, RowY, (std::max)(0, Rect.X + Rect.Width - CursorX), RowHeight }, Rect)
			: FRect{ 0, 0, 0, 0 };
		LayoutDropdown.Rect = { 0, 0, 0, 0 };
}

bool SViewportToolbarWidget::HandleDropdownMouse(SDropdown& Dropdown, EDropdownId DropdownId, int32 X, int32 Y)
{
	const bool bHandled = Dropdown.OnMouseDown(X, Y);
	if (!bHandled)
	{
		return false;
	}

	if (Dropdown.IsOpen())
	{
		CloseOtherDropdowns(DropdownId);
	}

	SyncSelectionState();
	UpdateGeometry();
	return true;
}

void SViewportToolbarWidget::CloseAllDropdowns()
{
	LayoutDropdown.SetOpen(false);
	TypeDropdown.SetOpen(false);
	ModeDropdown.SetOpen(false);
}

void SViewportToolbarWidget::CloseOtherDropdowns(EDropdownId KeepOpen)
{
	if (KeepOpen != EDropdownId::Layout)
	{
		LayoutDropdown.SetOpen(false);
	}
	if (KeepOpen != EDropdownId::Type)
	{
		TypeDropdown.SetOpen(false);
	}
	if (KeepOpen != EDropdownId::Mode)
	{
		ModeDropdown.SetOpen(false);
	}
}

EViewportLayout SViewportToolbarWidget::GetCurrentLayout() const
{
	if (!Engine || !Engine->GetSlateApplication())
	{
		return EViewportLayout::Single;
	}

	return Engine->GetSlateApplication()->GetCurrentLayout();
}

FViewportEntry* SViewportToolbarWidget::GetFocusedEntry() const
{
	if (!Engine)
	{
		return nullptr;
	}

	FSlateApplication* Slate = Engine->GetSlateApplication();
	if (!Slate)
	{
		return nullptr;
	}

	const FViewportId FocusedId = Slate->GetFocusedViewportId();
	if (FocusedId == INVALID_VIEWPORT_ID)
	{
		return nullptr;
	}

	FViewportEntry* FocusedEntry = Engine->GetViewportRegistry().FindEntryByViewportID(FocusedId);
	if (!FocusedEntry || !FocusedEntry->bActive)
	{
		return nullptr;
	}

	return FocusedEntry;
}

FViewportEntry* SViewportToolbarWidget::GetTargetEntry() const
{
	if (TargetViewportId == INVALID_VIEWPORT_ID)
	{
		return GetFocusedEntry();
	}

	if (!Engine)
	{
		return nullptr;
	}

	FViewportEntry* Entry = Engine->GetViewportRegistry().FindEntryByViewportID(TargetViewportId);
	return (Entry && Entry->bActive) ? Entry : nullptr;
}

void SViewportToolbarWidget::ApplyLayout(EViewportLayout NewLayout)
{
	if (!Engine || !Engine->GetSlateApplication())
	{
		return;
	}

	FSlateApplication* Slate = Engine->GetSlateApplication();
	Slate->SetLayout(NewLayout);

	const int32 ActiveViewportCount = Slate->GetActiveViewportCount();
	int32 EntryIndex = 0;
	for (FViewportEntry& Entry : Engine->GetViewportRegistry().GetEntries())
	{
		Entry.bActive = (EntryIndex < ActiveViewportCount);
		++EntryIndex;
	}
}

void SViewportToolbarWidget::ApplyViewportType(EViewportType NewType)
{
	if (!Engine)
	{
		return;
	}

	if (FViewportEntry* Entry = GetTargetEntry())
	{
		Engine->GetViewportRegistry().SetViewportType(Entry->Id, NewType);
	}
}

void SViewportToolbarWidget::ApplyRenderMode(ERenderMode NewMode)
{
	if (FViewportEntry* Entry = GetTargetEntry())
	{
		Entry->LocalState.ViewMode = NewMode;
	}
}

bool SViewportToolbarWidget::ShouldShowPIECaptureHint() const
{
	if (!Engine || !Engine->IsPIEActive() || !Engine->IsPIEInputCaptured())
	{
		return false;
	}

	const FViewportEntry* Entry = GetTargetEntry();
	return Entry &&
		Entry->WorldContext &&
		Entry->WorldContext->WorldType == EWorldType::PIE;
}
