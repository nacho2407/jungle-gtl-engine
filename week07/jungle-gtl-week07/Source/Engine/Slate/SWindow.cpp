#include "SWindow.h"

bool SWindow::IsHover(FPoint Coord) const
{
	return false;
}

SWidget* SWindow::HitTest(int32 X, int32 Y)
{
	if (!Rect.Contains(static_cast<float>(X), static_cast<float>(Y))) 
		return nullptr;

	if (Child)
	{
		SWidget* Hit = Child->HitTest(X, Y);
		if (Hit) return Hit;
	}

	return this;
}
