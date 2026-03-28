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
	int X = 0;
	int Y = 0;
	int Width = 0;
	int Height = 0;

	bool Contains(int Px, int Py) const
	{
		return (Px >= X && Px <= X + Width && Py >= Y && Py <= Y + Py);
	}

	void WindowToLocal(int Px, int Py, int& OutX, int& OutY) const
	{
		if (Contains(Px, Py))
		{
			OutX = Px - X;
			OutY = Py - Y;
		}
	}

	void WindowToNormalized(int Px, int Py, float& OutU, float& OutV) const
	{
		int U = 0, V = 0;
		WindowToLocal(Px, Py, U, V);
		OutU = static_cast<float>(U) / static_cast<float>(Width);
		OutV = static_cast<float>(V) / static_cast<float>(Height);
	}
};

struct FViewportMouseEvent
{
	int WindowX = 0;
	int WindowY = 0;

	int LocalX = 0;
	int LocalY = 0;

	int DeltaX = 0;
	int DeltaY = 0;

	bool bLeftDown = false;
	bool bRightDown = false;
	bool bMiddleDown = false;
};
