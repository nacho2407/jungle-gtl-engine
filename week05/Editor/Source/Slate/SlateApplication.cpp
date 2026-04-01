#include "SlateApplication.h"
#include "Viewport/Viewport.h"
#include <algorithm>
#include <utility>

// ────────────────────────────────────────────────────────────
// Initialize
// ────────────────────────────────────────────────────────────
void FSlateApplication::Initialize(const FRect& Area, FViewport* VPs[], int32 Count)
{
	for (int i = 0; i < MAX_VIEWPORTS; i++)
	{
		Viewports[i]          = std::make_unique<SViewport>();
		Viewports[i]->Id      = i;
		Viewports[i]->Viewport = (i < Count) ? VPs[i] : nullptr;
	}

	AreaRect = Area;
	SetLayout(EViewportLayout::FourGrid);
}

void FSlateApplication::ResetPools()
{
	for (auto& S : SplitterPool_H) S = std::make_unique<SSplitterH>();
	for (auto& S : SplitterPool_V) S = std::make_unique<SSplitterV>();
	for (auto& S : ActiveSplitters) S = nullptr;
	ActiveSplitterCount = 0;
	Root = nullptr;
}

void FSlateApplication::SyncViewportRects()
{
	for (int i = 0; i < ActiveViewportCount; i++)
	{
		if (Viewports[i] && Viewports[i]->Viewport)
			Viewports[i]->Viewport->SetRect(Viewports[i]->Rect);
	}
}

// ────────────────────────────────────────────────────────────
// SetLayout
// ────────────────────────────────────────────────────────────
void FSlateApplication::SetLayout(EViewportLayout Layout)
{
	if (bViewportMaximized && Layout != EViewportLayout::Single)
	{
		if (SwappedViewportIndex > 0 && SwappedViewportIndex < MAX_VIEWPORTS)
		{
			std::swap(Viewports[0], Viewports[SwappedViewportIndex]);
		}

		bViewportMaximized = false;
		MaximizedViewportId = INVALID_VIEWPORT_ID;
		SwappedViewportIndex = -1;
	}

	CurrentLayout = Layout;
	ResetPools();

	switch (Layout)
	{
	case EViewportLayout::Single:      BuildTree_Single();      break;
	case EViewportLayout::SplitH:      BuildTree_SplitH();      break;
	case EViewportLayout::SplitV:      BuildTree_SplitV();      break;
	case EViewportLayout::ThreeLeft:   BuildTree_ThreeLeft();   break;
	case EViewportLayout::ThreeRight:  BuildTree_ThreeRight();  break;
	case EViewportLayout::ThreeTop:    BuildTree_ThreeTop();    break;
	case EViewportLayout::ThreeBottom: BuildTree_ThreeBottom(); break;
	case EViewportLayout::FourGrid:    BuildTree_FourGrid();    break;
	}

	PerformLayout();
}

// ────────────────────────────────────────────────────────────
// BuildTree 구현
//   H-Splitter: SideLT=왼쪽, SideRB=오른쪽
//   V-Splitter: SideLT=위쪽,  SideRB=아래쪽
// ────────────────────────────────────────────────────────────
void FSlateApplication::BuildTree_Single()
{
	// [VP0]
	ActiveViewportCount = 1;
	Root = Viewports[0].get();
}

void FSlateApplication::BuildTree_SplitH()
{
	// [VP0 | VP1]
	ActiveViewportCount = 2;
	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(Viewports[0].get());
	SplitterPool_H[0]->SetSideRB(Viewports[1].get());
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	Root = SplitterPool_H[0].get();
}

void FSlateApplication::BuildTree_SplitV()
{
	// [VP0]
	// [VP1]
	ActiveViewportCount = 2;
	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(Viewports[0].get());
	SplitterPool_V[0]->SetSideRB(Viewports[1].get());
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	Root = SplitterPool_V[0].get();
}

void FSlateApplication::BuildTree_ThreeLeft()
{
	// [VP0 | VP1]
	//      [ VP2]
	ActiveViewportCount = 3;
	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(Viewports[1].get());
	SplitterPool_V[0]->SetSideRB(Viewports[2].get());

	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(Viewports[0].get());
	SplitterPool_H[0]->SetSideRB(SplitterPool_V[0].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	Root = SplitterPool_H[0].get();
}

void FSlateApplication::BuildTree_ThreeRight()
{
	// [VP0 | VP2]
	// [VP1     ]
	ActiveViewportCount = 3;
	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(Viewports[0].get());
	SplitterPool_V[0]->SetSideRB(Viewports[1].get());

	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(SplitterPool_V[0].get());
	SplitterPool_H[0]->SetSideRB(Viewports[2].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	Root = SplitterPool_H[0].get();
}

void FSlateApplication::BuildTree_ThreeTop()
{
	// [   VP0   ]
	// [VP1 | VP2]
	ActiveViewportCount = 3;
	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(Viewports[1].get());
	SplitterPool_H[0]->SetSideRB(Viewports[2].get());

	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(Viewports[0].get());
	SplitterPool_V[0]->SetSideRB(SplitterPool_H[0].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	Root = SplitterPool_V[0].get();
}

void FSlateApplication::BuildTree_ThreeBottom()
{
	// [VP0 | VP1]
	// [   VP2   ]
	ActiveViewportCount = 3;
	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(Viewports[0].get());
	SplitterPool_H[0]->SetSideRB(Viewports[1].get());

	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(SplitterPool_H[0].get());
	SplitterPool_V[0]->SetSideRB(Viewports[2].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	Root = SplitterPool_V[0].get();
}

void FSlateApplication::BuildTree_FourGrid()
{
	// [VP0 | VP1]
	// [VP2 | VP3]
	ActiveViewportCount = 4;
	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(Viewports[0].get());
	SplitterPool_V[0]->SetSideRB(Viewports[2].get());

	SplitterPool_V[1]->Ratio  = 0.5f;
	SplitterPool_V[1]->SetSideLT(Viewports[1].get());
	SplitterPool_V[1]->SetSideRB(Viewports[3].get());

	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(SplitterPool_V[0].get());
	SplitterPool_H[0]->SetSideRB(SplitterPool_V[1].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[1].get();
	Root = SplitterPool_H[0].get();
}

// ────────────────────────────────────────────────────────────
// Layout
// ────────────────────────────────────────────────────────────
void FSlateApplication::SetViewportAreaRect(const FRect& Area)
{
	AreaRect = Area;
	PerformLayout();
}

void FSlateApplication::PerformLayout()
{
	if (!Root) return;
	Root->Rect = AreaRect;
	Root->ArrangeChildren();
	SyncViewportRects();
}

float FSlateApplication::GetSplitterRatio(int32 Index) const
{
	if (Index < ActiveSplitterCount && ActiveSplitters[Index])
	{
		return ActiveSplitters[Index]->Ratio;
	}
	return 0.5f;
}

bool FSlateApplication::IsViewportActive(FViewportId Id) const
{
	if (Id == INVALID_VIEWPORT_ID)
	{
		return false;
	}

	for (int32 i = 0; i < ActiveViewportCount; ++i)
	{
		if (Viewports[i] && Viewports[i]->Id == Id)
		{
			return true;
		}
	}

	return false;
}

void FSlateApplication::SetSplitterRatio(int32 Index, float Ratio)
{
	if (Index < ActiveSplitterCount)
	{
		ActiveSplitters[Index]->Ratio = Ratio;
	}
}

SWidget* FSlateApplication::CreateWidget(std::unique_ptr<SWidget> InWidget)
{
	SWidget* Raw = InWidget.get();
	OwnedWidgets.push_back(std::move(InWidget));
	return Raw;
}

void FSlateApplication::Paint(SWidget& Painter)
{
	if (Root) Root->Paint(Painter);

	if (FocusedViewportId != INVALID_VIEWPORT_ID)
	{
		for (int i = 0; i < ActiveViewportCount; i++)
		{
			if (!Viewports[i] || Viewports[i]->Id != FocusedViewportId)
			{
				continue;
			}

			const FRect FocusRect = Viewports[i]->Rect;
			if (!FocusRect.IsValid())
			{
				break;
			}

			const int32 Inset = 0;
			const FRect Outer = {
				FocusRect.X + Inset,
				FocusRect.Y + Inset,
				FocusRect.Width - Inset * 2,
				FocusRect.Height - Inset * 2
			};
			if (Outer.IsValid())
			{
				Painter.DrawRect(Outer, 0xFF00B7FF);
				const FRect Inner = { Outer.X + 1, Outer.Y + 1, Outer.Width - 2, Outer.Height - 2 };
				if (Inner.IsValid())
				{
					Painter.DrawRect(Inner, 0xFF00B7FF);
				}
			}
			break;
		}
	}

	for (auto* W : OverlayWidgets) W->Paint(Painter);
}

SWidget* FSlateApplication::FindTopOverlayWidgetAt(FPoint Point) const
{
	for (int32 i = static_cast<int32>(OverlayWidgets.size()) - 1; i >= 0; --i)
	{
		SWidget* Widget = OverlayWidgets[i];
		if (Widget && Widget->HitTest(Point))
		{
			return Widget;
		}
	}

	return nullptr;
}

void FSlateApplication::BringOverlayWidgetToFront(SWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	auto It = std::find(OverlayWidgets.begin(), OverlayWidgets.end(), Widget);
	if (It == OverlayWidgets.end() || std::next(It) == OverlayWidgets.end())
	{
		return;
	}

	OverlayWidgets.erase(It);
	OverlayWidgets.push_back(Widget);
}

// ────────────────────────────────────────────────────────────
// Mouse input
// ────────────────────────────────────────────────────────────
void FSlateApplication::ProcessMouseDown(int32 X, int32 Y)
{
	const FPoint Point{ X, Y };

	for (int32 i = static_cast<int32>(OverlayWidgets.size()) - 1; i >= 0; --i)
	{
		SWidget* W = OverlayWidgets[i];
		if (!W || !W->HitTest(Point))
		{
			continue;
		}

		if (W->OnMouseDown(X, Y))
		{
			BringOverlayWidgetToFront(W);
			return;
		}
	}
	// Splitter 바 히트 우선
	for (int i = 0; i < ActiveSplitterCount; i++)
	{
		SSplitter* S = ActiveSplitters[i];
		if (!S) continue;
		FRect Bar = S->GetSplitterBarRect();
		if (Bar.IsValid() &&
			Bar.X <= X && X <= Bar.X + Bar.Width &&
			Bar.Y <= Y && Y <= Bar.Y + Bar.Height)
		{
			DraggingSplitter = S;
			return;
		}
	}

	// Viewport 히트 → FocusedViewportId 갱신
	for (int i = 0; i < ActiveViewportCount; i++)
	{
		if (Viewports[i] && Viewports[i]->HitTest(X, Y))
		{
			FocusedViewportId = Viewports[i]->Id;
			return;
		}
	}
}

void FSlateApplication::ProcessMouseDoubleClick(int32 X, int32 Y)
{
	if (SWidget* Overlay = FindTopOverlayWidgetAt({ X, Y }))
	{
		BringOverlayWidgetToFront(Overlay);
		return;
	}

	for (int i = 0; i < ActiveViewportCount; i++)
	{
		if (!Viewports[i] || !Viewports[i]->HitTest(X, Y))
		{
			continue;
		}

		FocusedViewportId = Viewports[i]->Id;
		ToggleViewportMaximize(FocusedViewportId);
		return;
	}
}

void FSlateApplication::ProcessMouseMove(int32 X, int32 Y)
{
	IsCursorInArea = false;
	if (AreaRect.IsValid() && AreaRect.X < X && X < AreaRect.X + AreaRect.Width && AreaRect.Y < Y && Y < AreaRect.Y + AreaRect.Height)
		IsCursorInArea = true;


	for (int i = 0; i < ActiveSplitterCount; i++)
	{
		if (ActiveSplitters[i])
		{
			ActiveSplitters[i]->Color = 0xFF3C3C3C;
		}
	}

	if (DraggingSplitter)
	{
		DraggingSplitter->Color = 0xFF5A9CFF;
		HoveredViewportId = INVALID_VIEWPORT_ID;
		CurrentCursor = DraggingSplitter->GetCursor();
		DraggingSplitter->OnMouseMove(X, Y);
		PerformLayout();
		return;
	}

	if (SWidget* Overlay = FindTopOverlayWidgetAt({ X, Y }))
	{
		HoveredViewportId = INVALID_VIEWPORT_ID;
		CurrentCursor = Overlay->GetCursor();
		return;
	}

	for (int i = 0; i < ActiveSplitterCount; i++)
	{
		SSplitter* S = ActiveSplitters[i];
		if (!S) continue;
		FRect Bar = S->GetSplitterBarRect();
		if (Bar.IsValid() &&
			Bar.X <= X && X <= Bar.X + Bar.Width &&
			Bar.Y <= Y && Y <= Bar.Y + Bar.Height)
		{
			S->Color = 0xFF5A9CFF;
			HoveredViewportId = INVALID_VIEWPORT_ID;
			CurrentCursor = S->GetCursor();
			return;
		}
	}

	HoveredViewportId = INVALID_VIEWPORT_ID;
	for (int i = 0; i < ActiveViewportCount; i++)
	{
		if (Viewports[i] && Viewports[i]->HitTest(X, Y))
		{
			CurrentCursor = Viewports[i]->GetCursor();
			HoveredViewportId = Viewports[i]->Id;
			return;
		}
	}

	CurrentCursor = EMouseCursor::Default;
}

void FSlateApplication::ProcessMouseUp(int32 X, int32 Y)
{
	if (DraggingSplitter)
	{
		DraggingSplitter->Color = 0xFF3C3C3C;
		DraggingSplitter = nullptr;
		if (OnSplitterDragEnd) OnSplitterDragEnd();
	}
}

int32 FSlateApplication::FindActiveViewportIndexById(FViewportId ViewportId) const
{
	for (int32 i = 0; i < ActiveViewportCount; ++i)
	{
		if (Viewports[i] && Viewports[i]->Id == ViewportId)
		{
			return i;
		}
	}

	return -1;
}

void FSlateApplication::ToggleViewportMaximize(FViewportId ViewportId)
{
	if (ViewportId == INVALID_VIEWPORT_ID)
	{
		return;
	}

	if (bViewportMaximized)
	{
		const EViewportLayout RestoreLayout = LayoutBeforeMaximize;
		const bool bRestoreOnly = (ViewportId == MaximizedViewportId);

		if (SwappedViewportIndex > 0 && SwappedViewportIndex < MAX_VIEWPORTS)
		{
			std::swap(Viewports[0], Viewports[SwappedViewportIndex]);
		}

		bViewportMaximized = false;
		MaximizedViewportId = INVALID_VIEWPORT_ID;
		SwappedViewportIndex = -1;
		SetLayout(RestoreLayout);

		if (bRestoreOnly)
		{
			return;
		}
	}

	const int32 TargetIndex = FindActiveViewportIndexById(ViewportId);
	if (TargetIndex < 0)
	{
		return;
	}

	LayoutBeforeMaximize = CurrentLayout;
	MaximizedViewportId = ViewportId;
	SwappedViewportIndex = TargetIndex;

	if (TargetIndex > 0 && TargetIndex < MAX_VIEWPORTS)
	{
		std::swap(Viewports[0], Viewports[TargetIndex]);
	}
	else
	{
		SwappedViewportIndex = -1;
	}

	SetLayout(EViewportLayout::Single);
	FocusedViewportId = ViewportId;
	bViewportMaximized = true;
}
