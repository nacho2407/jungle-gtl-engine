#include "ViewportToolbar.h"

#include "EditorEngine.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Viewport/Viewport.h"
#include "Slate/SlateApplication.h"

#include <algorithm>
#include <limits>

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

	const TArray<FString>& GetViewportTypeOptions()
	{
		static const TArray<FString> Options = {
			"Perspective",
			"Top",
			"Front",
			"Right"
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
	Rect = { 0, 0, 330, HeaderHeight };
	TitleButton.Text = "Viewport";
	TitleButton.bEnabled = false;
	TitleButton.FontSize = 11.0f;
	TitleButton.DisabledBackgroundColor = 0xFF1F1F1F;
	TitleButton.BorderColor = 0xFF4E4E4E;
	TitleButton.DisabledTextColor = 0xFFBDBDBD;

	LayoutDropdown.Label = "Layout";
	LayoutDropdown.Placeholder = "Select";
	LayoutDropdown.SetOptions(GetLayoutOptions());
	LayoutDropdown.OnSelectionChanged = [this](int32 SelectedIndex)
	{
		ApplyLayout(static_cast<EViewportLayout>(SelectedIndex));
	};

	TypeDropdown.Label = "Type";
	TypeDropdown.Placeholder = "";
	TypeDropdown.SetOptions(GetViewportTypeOptions());
	TypeDropdown.OnSelectionChanged = [this](int32 SelectedIndex)
	{
		ApplyViewportType(static_cast<EViewportType>(SelectedIndex));
	};

	ModeDropdown.Label = "Mode";
	ModeDropdown.Placeholder = "";
	ModeDropdown.SetOptions(GetRenderModeOptions());
	ModeDropdown.OnSelectionChanged = [this](int32 SelectedIndex)
	{
		ApplyRenderMode(static_cast<ERenderMode>(SelectedIndex));
	};
}

void SViewportToolbarWidget::OnPaint(SWidget& Painter)
{
	SyncSelectionState();
	UpdateGeometry();

	Painter.DrawRectFilled(Rect, 0xD01C1E21);
	Painter.DrawRect(Rect, 0xFF555B63);

	TitleButton.Paint(Painter);
	LayoutDropdown.Paint(Painter);
	TypeDropdown.Paint(Painter);
	ModeDropdown.Paint(Painter);
}

bool SViewportToolbarWidget::OnMouseDown(int32 X, int32 Y)
{
	SyncSelectionState();
	UpdateGeometry();

	const FPoint Point{ X, Y };
	if (!HitTest(Point))
	{
		CloseAllDropdowns();
		UpdateGeometry();
		return false;
	}

	if (HandleDropdownMouse(LayoutDropdown, EDropdownId::Layout, X, Y))
	{
		return true;
	}

	if (HandleDropdownMouse(TypeDropdown, EDropdownId::Type, X, Y))
	{
		return true;
	}

	if (HandleDropdownMouse(ModeDropdown, EDropdownId::Mode, X, Y))
	{
		return true;
	}

	CloseAllDropdowns();
	UpdateGeometry();
	return true;
}

bool SViewportToolbarWidget::HitTest(FPoint Point) const
{
	return ContainsPoint(GetExpandedInteractiveRect(), Point);
}

void SViewportToolbarWidget::SyncSelectionState()
{
	LayoutDropdown.bEnabled = true;
	LayoutDropdown.SetSelectedIndex(static_cast<int32>(GetCurrentLayout()));

	FViewportEntry* FocusedEntry = GetFocusedEntry();
	const bool bHasFocusedEntry = (FocusedEntry != nullptr);

	TypeDropdown.bEnabled = bHasFocusedEntry;
	ModeDropdown.bEnabled = bHasFocusedEntry;

	if (!bHasFocusedEntry)
	{
		TypeDropdown.SetSelectedIndex(-1);
		ModeDropdown.SetSelectedIndex(-1);
		TypeDropdown.SetOpen(false);
		ModeDropdown.SetOpen(false);
		return;
	}

	TypeDropdown.SetSelectedIndex(static_cast<int32>(FocusedEntry->LocalState.ProjectionType));
	ModeDropdown.SetSelectedIndex(static_cast<int32>(FocusedEntry->LocalState.ViewMode));
}

void SViewportToolbarWidget::UpdateGeometry()
{
	FRect HeaderRect;
	if (!ComputeHeaderRect(HeaderRect))
	{
		return;
	}

	Rect = HeaderRect;

	const int32 Padding = 8;
	const int32 Gap = 6;
	const int32 RowHeight = 24;
	const int32 MinTitleWidth = 72;
	const int32 MaxTitleWidth = 110;
	const int32 MinDropdownWidth = 72;
	const int32 MinHeaderInnerWidth = MinTitleWidth + Gap * 3 + MinDropdownWidth * 3;

	const int32 InnerWidth = Rect.Width - Padding * 2;
	if (InnerWidth < MinHeaderInnerWidth)
	{
		return;
	}

	int32 TitleWidth = Rect.Width / 5;
	TitleWidth = (std::clamp)(TitleWidth, MinTitleWidth, MaxTitleWidth);

	int32 DropdownAreaWidth = InnerWidth - TitleWidth - Gap * 3;
	int32 DropdownWidth = DropdownAreaWidth / 3;
	if (DropdownWidth < MinDropdownWidth)
	{
		TitleWidth = (std::max)(MinTitleWidth, TitleWidth - (MinDropdownWidth - DropdownWidth) * 3);
		DropdownAreaWidth = InnerWidth - TitleWidth - Gap * 3;
		DropdownWidth = DropdownAreaWidth / 3;
		if (DropdownWidth < MinDropdownWidth)
		{
			return;
		}
	}

	TitleButton.Rect = { Rect.X + Padding, Rect.Y + (Rect.Height - RowHeight) / 2, TitleWidth, RowHeight };
	const int32 RowY = Rect.Y + (Rect.Height - RowHeight) / 2;
	int32 CursorX = TitleButton.Rect.X + TitleWidth + Gap;

	LayoutDropdown.Rect = { CursorX, RowY, DropdownWidth, RowHeight };
	CursorX += DropdownWidth + Gap;

	TypeDropdown.Rect = { CursorX, RowY, DropdownWidth, RowHeight };
	CursorX += DropdownWidth + Gap;

	ModeDropdown.Rect = { CursorX, RowY, DropdownWidth, RowHeight };
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

const FViewportEntry* SViewportToolbarWidget::GetAnchorEntry() const
{
	if (FViewportEntry* Focused = GetFocusedEntry())
	{
		return Focused;
	}

	if (!Engine)
	{
		return nullptr;
	}

	for (const FViewportEntry& Entry : Engine->GetViewportRegistry().GetEntries())
	{
		if (Entry.bActive)
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool SViewportToolbarWidget::ComputeHeaderRect(FRect& OutRect) const
{
	if (!Engine)
	{
		return false;
	}

	int32 MinX = (std::numeric_limits<int32>::max)();
	int32 MinY = (std::numeric_limits<int32>::max)();
	int32 MaxX = (std::numeric_limits<int32>::min)();
	bool bFound = false;

	for (const FViewportEntry& Entry : Engine->GetViewportRegistry().GetEntries())
	{
		if (!Entry.bActive || !Entry.Viewport)
		{
			continue;
		}

		const FRect& ViewRect = Entry.Viewport->GetRect();
		if (!ViewRect.IsValid())
		{
			continue;
		}

		bFound = true;
		MinX = (std::min)(MinX, ViewRect.X);
		MinY = (std::min)(MinY, ViewRect.Y);
		MaxX = (std::max)(MaxX, ViewRect.X + ViewRect.Width);
	}

	if (!bFound)
	{
		return false;
	}

	const int32 HeaderY = (std::max)(0, MinY - HeaderHeight);
	OutRect = { MinX, HeaderY, MaxX - MinX, HeaderHeight };
	return true;
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
	FViewportEntry* FocusedEntry = GetFocusedEntry();
	if (!FocusedEntry || !Engine)
	{
		return;
	}

	Engine->GetViewportRegistry().SetViewportType(FocusedEntry->Id, NewType);
}

void SViewportToolbarWidget::ApplyRenderMode(ERenderMode NewMode)
{
	FViewportEntry* FocusedEntry = GetFocusedEntry();
	if (!FocusedEntry)
	{
		return;
	}

	FocusedEntry->LocalState.ViewMode = NewMode;
}

bool SViewportToolbarWidget::ContainsPoint(const FRect& InRect, FPoint Point)
{
	return InRect.IsValid() &&
		(InRect.X <= Point.X && Point.X <= InRect.X + InRect.Width) &&
		(InRect.Y <= Point.Y && Point.Y <= InRect.Y + InRect.Height);
}

FRect SViewportToolbarWidget::UnionRects(const FRect& A, const FRect& B)
{
	if (!A.IsValid())
	{
		return B;
	}
	if (!B.IsValid())
	{
		return A;
	}

	const int32 Left = (std::min)(A.X, B.X);
	const int32 Top = (std::min)(A.Y, B.Y);
	const int32 Right = (std::max)(A.X + A.Width, B.X + B.Width);
	const int32 Bottom = (std::max)(A.Y + A.Height, B.Y + B.Height);
	return { Left, Top, Right - Left, Bottom - Top };
}

FRect SViewportToolbarWidget::GetExpandedInteractiveRect() const
{
	FRect Expanded = Rect;
	Expanded = UnionRects(Expanded, LayoutDropdown.GetExpandedRect());
	Expanded = UnionRects(Expanded, TypeDropdown.GetExpandedRect());
	Expanded = UnionRects(Expanded, ModeDropdown.GetExpandedRect());
	return Expanded;
}
