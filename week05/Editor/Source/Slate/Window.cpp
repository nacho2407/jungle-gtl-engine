#include "Window.h"

bool SWindow::IsHover(FPoint coord) const
{
	if (!Rect.IsValid()) return false;

	return (Rect.X < coord.X && coord.X < Rect.X + Rect.Width &&
		Rect.Y < coord.Y && coord.Y < Rect.Y + Rect.Height);
}