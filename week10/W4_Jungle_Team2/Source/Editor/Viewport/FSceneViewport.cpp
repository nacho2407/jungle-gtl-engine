#include "FSceneViewport.h"


void FSceneViewport::Draw()
{
}

const FViewportRect& FSceneViewport::GetRect() const
{
	return FViewportRect();
}

bool FSceneViewport::ContainsPoint(int X, int Y) const
{
	return false;
}

void FSceneViewport::WindowToLocal(int X, int Y, int& OutX, int& OutY) const
{
}

bool FSceneViewport::OnMouseMove(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseButtonDown(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseButtonUp(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseWheel(float Delta)
{
	return false;
}

bool FSceneViewport::OnKeyDown(uint32 Key)
{
	return false;
}

bool FSceneViewport::OnKeyUp(uint32 Key)
{
	return false;
}

bool FSceneViewport::OnChar(uint32 Codepoint)
{
	return false;
}
