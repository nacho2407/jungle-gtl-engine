#include "SViewport.h"

bool SViewport::OnMouseMove(int X, int Y)
{
	return false;
}

bool SViewport::OnMouseButtonDown(int Button, int X, int Y)
{
	return false;
}

bool SViewport::OnMouseButtonUp(int Button, int X, int Y)
{
	return false;
}

bool SViewport::OnMouseWheel(int Delta, int X, int Y)
{
	return false;
}

bool SViewport::OnKeyDown(uint32 Key)
{
	return false;
}

bool SViewport::OnKeyUp(uint32 Key)
{
	return false;
}

FViewportMouseEvent SViewport::MakeMouseEvent(int X, int Y) const
{
	return FViewportMouseEvent();
}
