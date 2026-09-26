#include "ViewportHost.h"
#include <algorithm>

FVector2 SViewportHost::ComputeMinSize() const
{
	const FVector2 ViewportMin = ViewportWidget ? ViewportWidget->ComputeMinSize() : FVector2{ 0.0f, 0.0f };
	const float SceneMinWidth = (std::max)(static_cast<float>(MinSceneWidth), ViewportMin.X);
	const float SceneMinHeight = (std::max)(static_cast<float>(MinSceneHeight), ViewportMin.Y);
	return { SceneMinWidth, SceneMinHeight + static_cast<float>(HeaderHeight) };
}

void SViewportHost::ArrangeChildren()
{
	if (!Rect.IsValid())
	{
		HeaderRect = { 0, 0, 0, 0 };
		if (ViewportWidget)
		{
			ViewportWidget->Rect = { 0, 0, 0, 0 };
		}
		return;
	}

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
