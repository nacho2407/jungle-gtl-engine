#include "SlateApplication.h"
#include "Viewport/Viewport.h"

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
	for (auto* W : OverlayWidgets) W->Paint(Painter);
}

// ────────────────────────────────────────────────────────────
// Mouse input
// ────────────────────────────────────────────────────────────
void FSlateApplication::ProcessMouseDown(int32 X, int32 Y)
{
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

void FSlateApplication::ProcessMouseMove(int32 X, int32 Y)
{
	if (DraggingSplitter)
	{
		CurrentCursor = DraggingSplitter->GetCursor();
		DraggingSplitter->OnMouseMove(X, Y);
		PerformLayout();
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
		DraggingSplitter = nullptr;
		if (OnSplitterDragEnd) OnSplitterDragEnd();
	}
}
