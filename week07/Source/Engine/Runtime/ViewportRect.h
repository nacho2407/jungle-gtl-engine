#pragma once
#include "Core/CoreTypes.h"

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