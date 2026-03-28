#include "Core/Runtime/Slate/Window/SSplitter.h"
#include "Core/Math/MathUtility.h"

#include <cmath>

// Deprecated: resizing is handled by WindowOverlayManager::ResetViewportDimension.
void SSplitter::OnResize(float Width, float Height) {}

void SSplitterV::OnDrag(float Delta, float MinBound, float MaxBound)
{
    if (std::abs(Delta) < FMath::KindaSmallNumber)
    {
        return;
    }

    Origin.X = FMath::Clamp(Origin.X + Delta, MinBound, MaxBound);
    ResetPanelDimension();
}

void SSplitterH::OnDrag(float Delta, float MinBound, float MaxBound)
{
    if (std::abs(Delta) < FMath::KindaSmallNumber)
    {
        return;
    }

    Origin.Y = FMath::Clamp(Origin.Y + Delta, MinBound, MaxBound);
    ResetPanelDimension();
}

void SSplitterV::ResetPanelDimension()
{
    const float SplitX = Origin.X;

    for (SWindow* Panel : LeftPanels)
    {
        if (!Panel)
            continue;
        Panel->PosX = 0.f;
        Panel->Width = SplitX;
    }

    for (SWindow* Panel : RightPanels)
    {
        if (!Panel)
            continue;
        Panel->PosX = SplitX;
        Panel->Width = WindowWidth - SplitX;
    }
}

void SSplitterH::ResetPanelDimension()
{
    const float SplitY = Origin.Y;

    for (SWindow* Panel : UpPanels)
    {
        if (!Panel)
            continue;
        Panel->PosY = 0.f;
        Panel->Height = SplitY;
    }

    for (SWindow* Panel : BottomPanels)
    {
        if (!Panel)
            continue;
        Panel->PosY = SplitY;
        Panel->Height = WindowHeight - SplitY;
    }
}
