#include "ViewportHost.h"

void SViewportHost::ArrangeChildren()
{
	const int32 ActualHeaderHeight = (std::min)(HeaderHeight, (std::max)(0, Rect.Height));
	HeaderRect = { Rect.X, Rect.Y, Rect.Width, ActualHeaderHeight };
	if (ViewportWidget)
	{
		ViewportWidget->Rect = { Rect.X, Rect.Y + ActualHeaderHeight, Rect.Width, (std::max)(0, Rect.Height - ActualHeaderHeight) };
		ViewportWidget->ArrangeChildren();
	}
}

void SViewportHost::OnPaint(FSlatePaintContext& Painter)
{
	if (ViewportWidget)
	{
		ViewportWidget->Paint(Painter);
	}
}

bool SViewportHost::OnMouseDown(int32 X, int32 Y)
{
	return ViewportWidget ? ViewportWidget->OnMouseDown(X, Y) : false;
}

