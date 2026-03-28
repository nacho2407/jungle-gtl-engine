#include "SSplitter.h"
#include "Core/Math/MathUtility.h"

void SSplitter::OnResize(float Width, float Height) 
{
    // Assume that resizing splitters have already been done by WindowOverlayManager
    WindowWidth = Width;
    WindowHeight = Height;

    ResetPanelDimension();
}

void SSplitterV::OnDrag(float Delta, float MinBound, float MaxBound)
{ 
	if (Delta < FMath::KindaSmallNumber)
    {
		return;
    }

    Origin.X += Delta;
    FMath::Clamp(Origin.X, MinBound, MaxBound);

    ResetPanelDimension();
}

void SSplitterH::OnDrag(float Delta, float MinBound, float MaxBound)
{
    if (Delta < FMath::KindaSmallNumber)
    {
        return;
    }

    Origin.Y += Delta;
    FMath::Clamp(Origin.Y, MinBound, MaxBound);

    ResetPanelDimension();
}

void SSplitterV::ResetPanelDimension()
{

    for (FEditorViewportPanel* Panel : LeftPanels)
    {
    }

    for (FEditorViewportPanel* Panel : RightPanels)
    {
    }
}

void SSplitterH::ResetPanelDimension() 
{
    for (FEditorViewportPanel* Panel : UpPanels)
    {
    }

    for (FEditorViewportPanel* Panel : BottomPanels)
    {
    }
}