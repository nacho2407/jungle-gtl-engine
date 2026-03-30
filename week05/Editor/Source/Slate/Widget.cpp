#include "Widget.h"

bool SWidget::IsHover(FPoint Point) const
{
	if (!Rect.IsValid()) return false;

	return (Rect.X < Point.X && Point.X < Rect.X + Rect.Width &&
		Rect.Y < Point.Y && Point.Y < Rect.Y + Rect.Height);
}