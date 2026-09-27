#include "UI/SWindow.h"

bool SWindow::IsHover(FPoint coord) const
{
	return coord.X >= Rect.X && coord.X <= Rect.X + Rect.Width
		&& coord.Y >= Rect.Y && coord.Y <= Rect.Y + Rect.Height;
}
