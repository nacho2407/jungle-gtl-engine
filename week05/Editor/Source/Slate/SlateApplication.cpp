#include "SlateApplication.h"
#include "Viewport/Viewport.h"
#include <algorithm>
#include <utility>

void FSlateApplication::Initialize(const FRect& Area, FViewport* VPs[], int32 Count)
{
	for (int32 i = 0; i < MAX_VIEWPORTS; i++)
	{
		Viewports[i] = std::make_unique<SViewport>();
		ViewportHosts[i] = std::make_unique<SViewportHost>();
		Viewports[i]->Id = i;
		Viewports[i]->Viewport = (i < Count) ? VPs[i] : nullptr;
		ViewportHosts[i]->SetViewportWidget(Viewports[i].get());
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
	for (int32 i = 0; i < ActiveViewportCount; i++)
	{
		if (Viewports[i] && Viewports[i]->Viewport)
		{
			Viewports[i]->Viewport->SetRect(Viewports[i]->Rect);
		}
	}
}

void FSlateApplication::RefreshChromeWidgetList()
{
	ChromePaintOrder.clear();
	if (GlobalChromeWidget)
	{
		ChromePaintOrder.push_back(GlobalChromeWidget);
	}

	for (int32 ViewportId = 0; ViewportId < MAX_VIEWPORTS; ++ViewportId)
	{
		if (ViewportChromeWidgets[ViewportId])
		{
			ChromePaintOrder.push_back(ViewportChromeWidgets[ViewportId]);
		}
	}
}

int32 FSlateApplication::GetGlobalToolbarHeight() const
{
	return GlobalChromeWidget ? static_cast<int32>(GlobalChromeWidget->ComputeDesiredSize().Y + 0.5f) : 0;
}

FRect FSlateApplication::GetWorkspaceRect() const
{
	const int32 GlobalHeight = GetGlobalToolbarHeight();
	return { AreaRect.X, AreaRect.Y + GlobalHeight, AreaRect.Width, (std::max)(0, AreaRect.Height - GlobalHeight) };
}

void FSlateApplication::LayoutChromeWidgets()
{
	if (GlobalChromeWidget)
	{
		GlobalChromeWidget->Rect = { AreaRect.X, AreaRect.Y, AreaRect.Width, GetGlobalToolbarHeight() };
		GlobalChromeWidget->ArrangeChildren();
	}

	for (int32 ViewportId = 0; ViewportId < MAX_VIEWPORTS; ++ViewportId)
	{
		if (ViewportChromeWidgets[ViewportId])
		{
			ViewportChromeWidgets[ViewportId]->Rect = { 0, 0, 0, 0 };
		}
	}

	for (int32 i = 0; i < ActiveViewportCount; ++i)
	{
		if (!ViewportHosts[i])
		{
			continue;
		}

		if (SWidget* Chrome = ViewportChromeWidgets[Viewports[i]->Id])
		{
			Chrome->Rect = ViewportHosts[i]->GetHeaderRect();
			Chrome->ArrangeChildren();
		}
	}
}

void FSlateApplication::SetLayout(EViewportLayout Layout)
{
	if (bViewportMaximized && Layout != EViewportLayout::Single)
	{
		if (SwappedViewportIndex > 0 && SwappedViewportIndex < MAX_VIEWPORTS)
		{
			std::swap(Viewports[0], Viewports[SwappedViewportIndex]);
			std::swap(ViewportHosts[0], ViewportHosts[SwappedViewportIndex]);
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

	if (!IsViewportActive(FocusedViewportId))
	{
		FocusedViewportId = (ActiveViewportCount > 0 && Viewports[0]) ? Viewports[0]->Id : INVALID_VIEWPORT_ID;
	}

	if (!IsViewportActive(HoveredViewportId))
	{
		HoveredViewportId = INVALID_VIEWPORT_ID;
	}
}

void FSlateApplication::FocusViewport(FViewportId ViewportId)
{
	if (IsViewportActive(ViewportId))
	{
		FocusedViewportId = ViewportId;
		return;
	}

	FocusedViewportId = (ActiveViewportCount > 0 && Viewports[0]) ? Viewports[0]->Id : INVALID_VIEWPORT_ID;
}

// ────────────────────────────────────────────────────────────
// BuildTree 구현
//   H-Splitter: SideLT=왼쪽, SideRB=오른쪽
//   V-Splitter: SideLT=위쪽,  SideRB=아래쪽
// ────────────────────────────────────────────────────────────
void FSlateApplication::BuildTree_Single()
{
	ActiveViewportCount = 1;
	Root = ViewportHosts[0].get();
}

void FSlateApplication::BuildTree_SplitH()
{
	ActiveViewportCount = 2;
	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(ViewportHosts[0].get());
	SplitterPool_H[0]->SetSideRB(ViewportHosts[1].get());
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	Root = SplitterPool_H[0].get();
}

void FSlateApplication::BuildTree_SplitV()
{
	ActiveViewportCount = 2;
	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(ViewportHosts[0].get());
	SplitterPool_V[0]->SetSideRB(ViewportHosts[1].get());
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	Root = SplitterPool_V[0].get();
}

void FSlateApplication::BuildTree_ThreeLeft()
{
	ActiveViewportCount = 3;
	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(ViewportHosts[1].get());
	SplitterPool_V[0]->SetSideRB(ViewportHosts[2].get());

	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(ViewportHosts[0].get());
	SplitterPool_H[0]->SetSideRB(SplitterPool_V[0].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	Root = SplitterPool_H[0].get();
}

void FSlateApplication::BuildTree_ThreeRight()
{
	ActiveViewportCount = 3;
	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(ViewportHosts[0].get());
	SplitterPool_V[0]->SetSideRB(ViewportHosts[1].get());

	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(SplitterPool_V[0].get());
	SplitterPool_H[0]->SetSideRB(ViewportHosts[2].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	Root = SplitterPool_H[0].get();
}

void FSlateApplication::BuildTree_ThreeTop()
{
	ActiveViewportCount = 3;
	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(ViewportHosts[1].get());
	SplitterPool_H[0]->SetSideRB(ViewportHosts[2].get());

	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(ViewportHosts[0].get());
	SplitterPool_V[0]->SetSideRB(SplitterPool_H[0].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	Root = SplitterPool_V[0].get();
}

void FSlateApplication::BuildTree_ThreeBottom()
{
	ActiveViewportCount = 3;
	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(ViewportHosts[0].get());
	SplitterPool_H[0]->SetSideRB(ViewportHosts[1].get());

	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(SplitterPool_H[0].get());
	SplitterPool_V[0]->SetSideRB(ViewportHosts[2].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	Root = SplitterPool_V[0].get();
}

void FSlateApplication::BuildTree_FourGrid()
{
	ActiveViewportCount = 4;
	SplitterPool_V[0]->Ratio  = 0.5f;
	SplitterPool_V[0]->SetSideLT(ViewportHosts[0].get());
	SplitterPool_V[0]->SetSideRB(ViewportHosts[2].get());

	SplitterPool_V[1]->Ratio  = 0.5f;
	SplitterPool_V[1]->SetSideLT(ViewportHosts[1].get());
	SplitterPool_V[1]->SetSideRB(ViewportHosts[3].get());

	SplitterPool_H[0]->Ratio  = 0.5f;
	SplitterPool_H[0]->SetSideLT(SplitterPool_V[0].get());
	SplitterPool_H[0]->SetSideRB(SplitterPool_V[1].get());

	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_H[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[0].get();
	ActiveSplitters[ActiveSplitterCount++] = SplitterPool_V[1].get();
	Root = SplitterPool_H[0].get();
}

void FSlateApplication::SetViewportAreaRect(const FRect& Area)
{
	AreaRect = Area;
	PerformLayout();
}

void FSlateApplication::PerformLayout()
{
	if (!Root)
	{
		return;
	}

	Root->Rect = GetWorkspaceRect();
	Root->ArrangeChildren();
	SyncViewportRects();
	LayoutChromeWidgets();
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

void FSlateApplication::AddOverlayWidget(SWidget* W)
{
	if (!W)
	{
		return;
	}

	if (W->IsChromeProvider())
	{
		GlobalChromeWidget = W->GetGlobalChromeWidget();
		for (int32 ViewportId = 0; ViewportId < MAX_VIEWPORTS; ++ViewportId)
		{
			ViewportChromeWidgets[ViewportId] = W->GetViewportChromeWidget(ViewportId);
		}
		RefreshChromeWidgetList();
		PerformLayout();
		return;
	}

	OverlayWidgets.push_back(W);
}

void FSlateApplication::BuildDrawList(FSlatePaintContext& Painter)
{
	Paint(Painter);
}

void FSlateApplication::Paint(FSlatePaintContext& Painter)
{
	if (Root)
	{
		Root->Paint(Painter);
	}

	if (FocusedViewportId != INVALID_VIEWPORT_ID)
	{
		for (int32 i = 0; i < ActiveViewportCount; i++)
		{
			if (!Viewports[i] || Viewports[i]->Id != FocusedViewportId || !ViewportHosts[i])
			{
				continue;
			}

			const FRect FocusRect = Viewports[i]->Rect;
			if (!FocusRect.IsValid())
			{
				break;
			}

			Painter.DrawRect(FocusRect, 0xFF00B7FF);
			const FRect Inner = { FocusRect.X + 1, FocusRect.Y + 1, FocusRect.Width - 2, FocusRect.Height - 2 };
			if (Inner.IsValid())
			{
				Painter.DrawRect(Inner, 0xFF00B7FF);
			}
			break;
		}
	}

	for (SWidget* Widget : ChromePaintOrder)
	{
		if (Widget && Widget->Rect.IsValid())
		{
			Widget->Paint(Painter);
		}
	}

	for (SWidget* W : OverlayWidgets)
	{
		if (W && W->Rect.IsValid())
		{
			W->Paint(Painter);
		}
	}
}

SWidget* FSlateApplication::FindTopOverlayWidgetAt(FPoint Point) const
{
	for (int32 i = static_cast<int32>(OverlayWidgets.size()) - 1; i >= 0; --i)
	{
		SWidget* Widget = OverlayWidgets[i];
		if (Widget && Widget->Rect.IsValid() && Widget->HitTest(Point))
		{
			return Widget;
		}
	}
	return nullptr;
}

SWidget* FSlateApplication::FindTopChromeWidgetAt(FPoint Point) const
{
	for (int32 i = static_cast<int32>(ChromePaintOrder.size()) - 1; i >= 0; --i)
	{
		SWidget* Widget = ChromePaintOrder[i];
		if (Widget && Widget->Rect.IsValid() && Widget->HitTest(Point))
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

void FSlateApplication::BringChromeWidgetToFront(SWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	auto It = std::find(ChromePaintOrder.begin(), ChromePaintOrder.end(), Widget);
	if (It == ChromePaintOrder.end() || std::next(It) == ChromePaintOrder.end())
	{
		return;
	}

	ChromePaintOrder.erase(It);
	ChromePaintOrder.push_back(Widget);
}

void FSlateApplication::ProcessMouseDown(int32 X, int32 Y)
{
	const FPoint Point{ X, Y };

	if (!ChromePaintOrder.empty())
	{
		SWidget* TopChrome = FindTopChromeWidgetAt(Point);
		bool bChromeHandled = false;
		for (SWidget* W : ChromePaintOrder)
		{
			if (!W || !W->Rect.IsValid())
			{
				continue;
			}

			const bool bHandledByThis = W->OnMouseDown(X, Y);
			if (W == TopChrome && bHandledByThis)
			{
				bChromeHandled = true;
			}
		}

		if (bChromeHandled && TopChrome)
		{
			BringChromeWidgetToFront(TopChrome);
			return;
		}
	}

	for (int32 i = static_cast<int32>(OverlayWidgets.size()) - 1; i >= 0; --i)
	{
		SWidget* W = OverlayWidgets[i];
		if (!W || !W->Rect.IsValid() || !W->HitTest(Point))
		{
			continue;
		}

		if (W->OnMouseDown(X, Y))
		{
			BringOverlayWidgetToFront(W);
			return;
		}
	}

	for (int32 i = 0; i < ActiveSplitterCount; i++)
	{
		SSplitter* S = ActiveSplitters[i];
		if (!S) continue;
		FRect Bar = S->GetSplitterBarRect();
		if (Bar.IsValid() && Bar.X <= X && X <= Bar.X + Bar.Width && Bar.Y <= Y && Y <= Bar.Y + Bar.Height)
		{
			DraggingSplitter = S;
			return;
		}
	}

	for (int32 i = 0; i < ActiveViewportCount; i++)
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
	if (SWidget* Chrome = FindTopChromeWidgetAt({ X, Y }))
	{
		BringChromeWidgetToFront(Chrome);
		return;
	}

	if (SWidget* Overlay = FindTopOverlayWidgetAt({ X, Y }))
	{
		BringOverlayWidgetToFront(Overlay);
		return;
	}

	for (int32 i = 0; i < ActiveViewportCount; i++)
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
	{
		IsCursorInArea = true;
	}

	for (int32 i = 0; i < ActiveSplitterCount; i++)
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

	if (SWidget* Chrome = FindTopChromeWidgetAt({ X, Y }))
	{
		HoveredViewportId = INVALID_VIEWPORT_ID;
		CurrentCursor = Chrome->GetCursor();
		return;
	}

	if (SWidget* Overlay = FindTopOverlayWidgetAt({ X, Y }))
	{
		HoveredViewportId = INVALID_VIEWPORT_ID;
		CurrentCursor = Overlay->GetCursor();
		return;
	}

	for (int32 i = 0; i < ActiveSplitterCount; i++)
	{
		SSplitter* S = ActiveSplitters[i];
		if (!S) continue;
		FRect Bar = S->GetSplitterBarRect();
		if (Bar.IsValid() && Bar.X <= X && X <= Bar.X + Bar.Width && Bar.Y <= Y && Y <= Bar.Y + Bar.Height)
		{
			S->Color = 0xFF5A9CFF;
			HoveredViewportId = INVALID_VIEWPORT_ID;
			CurrentCursor = S->GetCursor();
			return;
		}
	}

	HoveredViewportId = INVALID_VIEWPORT_ID;
	for (int32 i = 0; i < ActiveViewportCount; i++)
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
	(void)X;
	(void)Y;
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
			std::swap(ViewportHosts[0], ViewportHosts[SwappedViewportIndex]);
		}

		bViewportMaximized = false;
		MaximizedViewportId = INVALID_VIEWPORT_ID;
		SwappedViewportIndex = -1;
		SetLayout(RestoreLayout);

		for (int32 i = 0; i < ActiveSplitterCount; i++)
		{
			if (ActiveSplitters[i])
			{
				ActiveSplitters[i]->Ratio = SavedSplitterRatios[i];
			}
		}
		PerformLayout();

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

	for (int32 i = 0; i < ActiveSplitterCount; i++)
	{
		if (ActiveSplitters[i])
		{
			SavedSplitterRatios[i] = ActiveSplitters[i]->Ratio;
		}
	}

	LayoutBeforeMaximize = CurrentLayout;
	MaximizedViewportId = ViewportId;
	SwappedViewportIndex = TargetIndex;

	if (TargetIndex > 0 && TargetIndex < MAX_VIEWPORTS)
	{
		std::swap(Viewports[0], Viewports[TargetIndex]);
		std::swap(ViewportHosts[0], ViewportHosts[TargetIndex]);
	}
	else
	{
		SwappedViewportIndex = -1;
	}

	SetLayout(EViewportLayout::Single);
	FocusedViewportId = ViewportId;
	bViewportMaximized = true;
}

