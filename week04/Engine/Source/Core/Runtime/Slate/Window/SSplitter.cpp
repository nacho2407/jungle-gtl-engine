#include "Core/Runtime/Slate/Window/SSplitter.h"
#include "Core/Math/MathUtility.h"

#include <cmath>

// Deprecated: resizing is handled by WindowOverlayManager::ResetViewportDimension.
void SSplitter::OnResize(float Width, float Height) {}

void SSplitter::OnMouseButtonDown(int32 MouseX, int32 MouseY) 
{
    if (HitTest(FVector2(MouseX, MouseY)))
    {
        bIsClicked = true;
    }
}

void SSplitter::OnMouseButtonUp(int32 MouseX, int32 MouseY) 
{
    bIsClicked = false;
}

void SSplitter::OnMouseMove(float DeltaX, float DeltaY) 
{
    // Find mouse delta and call OnDrag if bIsClicked
}

void SSplitterV::OnDrag(float Delta)
{
    if (std::abs(Delta) < FMath::KindaSmallNumber)
    {
        return;
    }

    const float AreaWidth = AreaMaxX - AreaMinX;
    const float Lo = (MinBound > 0.f) ? MinBound : (AreaMinX + AreaWidth * 0.1f);
    const float Hi = (MaxBound > 0.f) ? MaxBound : (AreaMinX + AreaWidth * 0.9f);
    PosX = FMath::Clamp(PosX + Delta, Lo, Hi);
    ResetPanelDimension();
}

void SSplitterH::OnDrag(float Delta)
{
    if (std::abs(Delta) < FMath::KindaSmallNumber)
    {
        return;
    }

    const float AreaHeight = AreaMaxY - AreaMinY;
    const float Lo = (MinBound > 0.f) ? MinBound : (AreaMinY + AreaHeight * 0.1f);
    const float Hi = (MaxBound > 0.f) ? MaxBound : (AreaMinY + AreaHeight * 0.9f);
    PosY = FMath::Clamp(PosY + Delta, Lo, Hi);
    ResetPanelDimension();
}

void SSplitterV::ResetPanelDimension()
{
    const float SplitX = PosX;

    for (SWindow* Panel : LeftPanels)
    {
        if (!Panel)
            continue;
        Panel->PosX = AreaMinX;
        Panel->Width = SplitX - AreaMinX;
    }

    for (SWindow* Panel : RightPanels)
    {
        if (!Panel)
            continue;
        Panel->PosX = SplitX;
        Panel->Width = AreaMaxX - SplitX;
    }
}

void SSplitterH::ResetPanelDimension()
{
    const float SplitY = PosY;

    for (SWindow* Panel : UpPanels)
    {
        if (!Panel)
            continue;
        Panel->PosY = AreaMinY;
        Panel->Height = SplitY - AreaMinY;
    }

    for (SWindow* Panel : BottomPanels)
    {
        if (!Panel)
            continue;
        Panel->PosY = SplitY;
        Panel->Height = AreaMaxY - SplitY;
    }
}
