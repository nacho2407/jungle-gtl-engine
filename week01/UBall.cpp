#include "UBall.hpp"

void UBall::Render(URenderer& renderer)
{
	renderer.UpdateConstant(Location, Angle, FVector::Splat(Radius), 1.0f, FVector(1, 1, 1));
	renderer.UpdateTexture(GetTexture());
	renderer.RenderRectangle();
}

void UBall::BorderClamp()
{
	if (Location.x < LeftBorder)
	{
		Location.x = LeftBorder;
	}
	if (Location.x > RightBorder)
	{
		Location.x = RightBorder;
	}
	if (Location.y < BottomBorder)
	{
		Location.y = BottomBorder;
	}
}
