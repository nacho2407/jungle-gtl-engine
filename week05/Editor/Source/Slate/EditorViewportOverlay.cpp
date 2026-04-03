#include "EditorViewportOverlay.h"

#include "EditorEngine.h"
#include "UI/EditorUI.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <limits>

namespace
{
	constexpr int32 OverlayRowHeight = 34;
	constexpr int32 OverlaySpacing = 8;
}

SEditorViewportOverlay::SEditorViewportOverlay(
	FEditorEngine* InEngine,
	FEditorUI* InEditorUI,
	FEditorViewportClient* InViewportClient)
	: Engine(InEngine)
	, EditorUI(InEditorUI)
	, Toolbar(InEngine)
	, Transform(InEngine, InViewportClient)
	, FpsWidget(InEngine)
{
}

void SEditorViewportOverlay::OnPaint(SWidget& Painter)
{
	UpdateLayout();

	if (ShouldShowFPS())
	{
		FpsWidget.Paint(Painter);
		return; // GameGem
	}
	Transform.Paint(Painter);
	Toolbar.Paint(Painter);
}

bool SEditorViewportOverlay::OnMouseDown(int32 X, int32 Y)
{
	UpdateLayout();

	if (Toolbar.HasOpenDropdown())
	{
		return Toolbar.OnMouseDown(X, Y);
	}

	if (Transform.OnMouseDown(X, Y))
	{
		return true;
	}

	return Toolbar.OnMouseDown(X, Y);
}

bool SEditorViewportOverlay::HitTest(FPoint Point) const
{
	UpdateLayout();
	return ContainsPoint(GetInteractiveRect(), Point);
}

void SEditorViewportOverlay::UpdateLayout() const
{
	FRect NewViewportBounds;
	if (!ComputeViewportBounds(NewViewportBounds))
	{
		ViewportBounds = { 0, 0, 0, 0 };
		Toolbar.SetHeaderRect({ 0, 0, 0, 0 });
		Transform.SetWidgetRect({ 0, 0, 0, 0 });
		FpsWidget.SetWidgetRect({ 0, 0, 0, 0 });
		return;
	}

	ViewportBounds = NewViewportBounds;

	const int32 HeaderY = (std::max)(0, ViewportBounds.Y - OverlayRowHeight);
	Toolbar.SetHeaderRect({ ViewportBounds.X, HeaderY, ViewportBounds.Width, OverlayRowHeight });

	const int32 TransformWidth = Transform.GetDesiredWidth();
	const int32 TransformX = ViewportBounds.X + ViewportBounds.Width - TransformWidth - Transform.GetRightPadding();
	Transform.SetWidgetRect({ TransformX, ViewportBounds.Y, TransformWidth, OverlayRowHeight });

	if (!ShouldShowFPS())
	{
		FpsWidget.SetWidgetRect({ 0, 0, 0, 0 });
		return;
	}

	FpsWidget.Refresh();
	const int32 FpsWidth = FpsWidget.GetDesiredWidth();
	const int32 FpsX = Transform.Rect.IsValid()
		? Transform.Rect.X
		: ViewportBounds.X + ViewportBounds.Width - FpsWidth;
	const int32 FpsY = Transform.Rect.IsValid()
		? Transform.Rect.Y + Transform.Rect.Height + OverlaySpacing
		: ViewportBounds.Y + OverlayRowHeight + OverlaySpacing;
	FpsWidget.SetWidgetRect({ FpsX, FpsY, FpsWidth, OverlayRowHeight });
}

bool SEditorViewportOverlay::ComputeViewportBounds(FRect& OutRect) const
{
	if (!Engine)
	{
		return false;
	}

	int32 MinX = (std::numeric_limits<int32>::max)();
	int32 MinY = (std::numeric_limits<int32>::max)();
	int32 MaxX = (std::numeric_limits<int32>::min)();
	int32 MaxY = (std::numeric_limits<int32>::min)();
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
		MaxY = (std::max)(MaxY, ViewRect.Y + ViewRect.Height);
	}

	if (!bFound)
	{
		return false;
	}

	OutRect = { MinX, MinY, MaxX - MinX, MaxY - MinY };
	return true;
}

bool SEditorViewportOverlay::ShouldShowFPS() const
{
	//return EditorUI && EditorUI->GetDebugState().FPS; GameGem
	return true;
}

FRect SEditorViewportOverlay::GetInteractiveRect() const
{
	FRect Interactive = UnionRects(Toolbar.GetInteractiveRect(), Transform.GetInteractiveRect());

	if (Toolbar.HasOpenDropdown())
	{
		if (ViewportBounds.IsValid())
		{
			Interactive = UnionRects(Interactive, ViewportBounds);
		}
	}

	return Interactive;
}

bool SEditorViewportOverlay::ContainsPoint(const FRect& InRect, FPoint Point)
{
	return InRect.IsValid()
		&& InRect.X <= Point.X && Point.X <= InRect.X + InRect.Width
		&& InRect.Y <= Point.Y && Point.Y <= InRect.Y + InRect.Height;
}

FRect SEditorViewportOverlay::UnionRects(const FRect& A, const FRect& B)
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
