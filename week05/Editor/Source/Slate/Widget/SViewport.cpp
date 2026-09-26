#include "SViewport.h"

bool SViewport::HitTest(int32 X, int32 Y) const
{
	return Rect.IsValid() && (Rect.X < X && X < Rect.X + Rect.Width) && (Rect.Y < Y && Y < Rect.Y + Rect.Height);
}
