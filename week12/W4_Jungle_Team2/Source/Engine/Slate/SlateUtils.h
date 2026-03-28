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

struct FViewportRect
{
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;

	FViewportRect() = default;
	FViewportRect(int32 InX, int32 InY, int32 InWidth, int32 InHeight)
		:X(InX), Y(InY), Width(InWidth), Height(InHeight) {
	}
	FViewportRect(int32 InX, int32 InY, int32 InWidthHeight)
		: FViewportRect(InX, InY, InWidthHeight, InWidthHeight) {
	}

	bool Contains(int32 Px, int32 Py) const
	{
		return (Px >= X && Px <= X + Width && Py >= Y && Py <= Y + Height);
	}

	void WindowToLocal(int32 Px, int32 Py, int32& OutX, int32& OutY) const
	{
		if (Contains(Px, Py))
		{
			OutX = Px - X;
			OutY = Py - Y;
		}
	}

	void WindowToNormalized(int32 Px, int32 Py, float& OutU, float& OutV) const
	{
		int32 U = 0, V = 0;
		WindowToLocal(Px, Py, U, V);
		OutU = static_cast<float>(U) / static_cast<float>(Width);
		OutV = static_cast<float>(V) / static_cast<float>(Height);
	}
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
