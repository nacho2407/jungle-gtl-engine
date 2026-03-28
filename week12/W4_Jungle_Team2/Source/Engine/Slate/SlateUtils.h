#pragma once
#include "Render/Common/ViewTypes.h"

struct FRect
{
	float X = 0;
	float Y = 0;
	float Width = 0;
	float Height = 0;

	FRect() = default;
	FRect(float InX, float InY, float InWidth, float InHeight)
		:X(InX), Y(InY), Width(InWidth), Height(InHeight) {
	}
	FRect(float InX, float InY, float InWidthHeight)
		: FRect(InX, InY, InWidthHeight, InWidthHeight) {
	}

	bool Contains(float Px, float Py) const
	{
		return (Px >= X && Px <= X + Width && Py >= Y && Py <= Y + Height);
	}
};

struct FPoint
{
	float X = 0;
	float Y = 0;

	FPoint() = default;
	FPoint(float InX, float InY) : X(InX), Y(InY) {}
};

struct FViewportMouseEvent
{
	int32 WindowX = 0;
	int32 WindowY = 0;

	int32 LocalX = 0;
	int32 LocalY = 0;

	int32 DeltaX = 0;
	int32 DeltaY = 0;

	bool bLeftDown = false;
	bool bRightDown = false;
	bool bMiddleDown = false;
};
