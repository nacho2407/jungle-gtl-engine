#include "SlateApplication.h"
#include "Viewport/Viewport.h"

void FSlateApplication::Initialize(const FRect& Area,
    FViewport* VP0, FViewport* VP1, FViewport* VP2, FViewport* VP3,
    float RatioH, float RatioVL, float RatioVR)
{
    // SViewport 4개 생성, Id와 FViewport* 캐시 연결
    for (int i = 0; i < 4; i++)
        Viewports[i] = std::make_unique<SViewport>();

    Viewports[0]->Id = 0; Viewports[0]->Viewport = VP0;
    Viewports[1]->Id = 1; Viewports[1]->Viewport = VP1;
    Viewports[2]->Id = 2; Viewports[2]->Viewport = VP2;
    Viewports[3]->Id = 3; Viewports[3]->Viewport = VP3;

    // SSplitterVL: 왼쪽 열 (VP0 상단 / VP2 하단)
    SplitterVL = std::make_unique<SSplitterV>();
    SplitterVL->Ratio  = RatioVL;
    SplitterVL->SideLT = Viewports[0].get();
    SplitterVL->SideRB = Viewports[2].get();

    // SSplitterVR: 오른쪽 열 (VP1 상단 / VP3 하단)
    SplitterVR = std::make_unique<SSplitterV>();
    SplitterVR->Ratio  = RatioVR;
    SplitterVR->SideLT = Viewports[1].get();
    SplitterVR->SideRB = Viewports[3].get();

    // SSplitterH: 좌/우 열 분할
    SplitterH = std::make_unique<SSplitterH>();
    SplitterH->Ratio  = RatioH;
    SplitterH->SideLT = SplitterVL.get();
    SplitterH->SideRB = SplitterVR.get();

    SetViewportAreaRect(Area);
}

void FSlateApplication::SetViewportAreaRect(const FRect& Area)
{
    if (SplitterH)
    {
        SplitterH->Rect = Area;
        PerformLayout();
    }
}

void FSlateApplication::PerformLayout()
{
    if (!SplitterH) return;

    // 레이아웃 계산: H → VL → VR 순서
    SplitterH->ArrangeChildren();
    if (SplitterVL) SplitterVL->ArrangeChildren();
    if (SplitterVR) SplitterVR->ArrangeChildren();

    // SViewport::Rect → FViewport::SetRect 동기화
    for (int i = 0; i < 4; i++)
    {
        if (Viewports[i] && Viewports[i]->Viewport)
            Viewports[i]->Viewport->SetRect(Viewports[i]->Rect);
    }
}

bool FSlateApplication::IsPointerOverViewport(FViewportId Id) const
{
    return HoveredViewportId == Id;
}

SViewport* FSlateApplication::GetViewportWidget(int32 Idx) const
{
    if (Idx >= 0 && Idx < 4) return Viewports[Idx].get();
    return nullptr;
}

void FSlateApplication::ProcessMouseDown(int32 X, int32 Y)
{
    // 1순위: SSplitter 바 히트 → 드래그 시작
    for (SSplitter* S : { (SSplitter*)SplitterH.get(),
                          (SSplitter*)SplitterVL.get(),
                          (SSplitter*)SplitterVR.get() })
    {
        if (!S) continue;
        FRect Bar = S->GetSplitterBarRect();
        if (!Bar.IsValid()) continue;
        if (Bar.X <= X && X <= Bar.X + Bar.Width &&
            Bar.Y <= Y && Y <= Bar.Y + Bar.Height)
        {
            DraggingSplitter = S;
            return;
        }
    }

    // 2순위: SViewport 히트 → FocusedViewportId 갱신
    for (int i = 0; i < 4; i++)
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
    // 드래그 중: splitter 이동 → 레이아웃 재계산
    if (DraggingSplitter)
    {
        DraggingSplitter->OnMouseMove(X, Y);
        PerformLayout();
        return;
    }

    // HoveredViewportId 갱신
    HoveredViewportId = INVALID_VIEWPORT_ID;
    for (int i = 0; i < 4; i++)
    {
        if (Viewports[i] && Viewports[i]->HitTest(X, Y))
        {
            HoveredViewportId = Viewports[i]->Id;
            return;
        }
    }
}

void FSlateApplication::ProcessMouseUp(int32 X, int32 Y)
{
    if (DraggingSplitter)
    {
        DraggingSplitter->OnMouseUp();
        DraggingSplitter = nullptr;
    }
}
