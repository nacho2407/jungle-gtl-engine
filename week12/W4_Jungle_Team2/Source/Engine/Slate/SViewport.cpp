#include "SViewport.h"

bool SViewport::OnMouseMove(int32 X, int32 Y)
{
	return false;
}

bool SViewport::OnMouseButtonDown(int32 Button, int32 X, int32 Y)
{
	return false;
}

bool SViewport::OnMouseButtonUp(int32 Button, int32 X, int32 Y)
{
	return false;
}

bool SViewport::OnMouseWheel(int32 Delta, int32 X, int32 Y)
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

FViewportMouseEvent SViewport::MakeMouseEvent(int32 X, int32 Y) const
{
	return FViewportMouseEvent();
}
