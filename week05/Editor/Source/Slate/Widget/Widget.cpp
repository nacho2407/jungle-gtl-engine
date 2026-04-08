#include "Widget.h"

bool SWidget::IsHover(FPoint Point) const
{
	return ContainsPoint(Rect, Point);
}
