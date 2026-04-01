#include "SSplitterCross.h"
#include "SSplitterV.h"
#include "SSplitterH.h"
#include "SSplitter.h"
#include "SlateApplication.h"

FRect SSplitterCross::GetCrossRect() const
{
    if (!SplitterV || !SplitterH) return FRect(0.f, 0.f, 0.f, 0.f);

    // VBar: SSplitterV 의 가로 바  → Y 위치와 Height(=BarThickness) 사용
    // HBar: SSplitterH 의 세로 바  → X 위치와 Width(=BarThickness)  사용
    const FRect VBar = SplitterV->GetBarRect();
    const FRect HBar = SplitterH->GetBarRect();

    return FRect(HBar.X, VBar.Y, HBar.Width, VBar.Height);
}

SWidget* SSplitterCross::HitTest(int32 X, int32 Y)
{
    const FRect Cross = GetCrossRect();
    if (Cross.Contains(static_cast<float>(X), static_cast<float>(Y)))
        return this;
    return nullptr;
}

bool SSplitterCross::OnMouseButtonDown(int32 Button, int32 X, int32 Y)
{
    if (Button != 0) return false;

    bDragging = true;
    FSlateApplication::Get().SetCapturedWidget(this);
    return true;
}

bool SSplitterCross::OnMouseMove(int32 X, int32 Y)
{
    if (!bDragging || !SplitterV || !SplitterH) return false;

    // 상하 비율 업데이트 (SSplitterV)
    const float NewVRatio = SplitterV->ComputeNewRatio(X, Y);
    SplitterV->SetSplitRatio(NewVRatio);
    SplitterV->UpdateChildRect();

    // 좌우 비율 업데이트 (SSplitterH + LinkedSplitter)
    const float NewHRatio = SplitterH->ComputeNewRatio(X, Y);
    SplitterH->SetSplitRatio(NewHRatio);
    SplitterH->UpdateChildRect();

    SSplitter* Linked = SplitterH->GetLinkedSplitter();
    if (Linked)
    {
        Linked->SetSplitRatio(NewHRatio);
        Linked->UpdateChildRect();
    }

    return true;
}

bool SSplitterCross::OnMouseButtonUp(int32 Button, int32 X, int32 Y)
{
    if (Button != 0 || !bDragging) return false;

    bDragging = false;
    FSlateApplication::Get().SetCapturedWidget(nullptr);
    return true;
}
