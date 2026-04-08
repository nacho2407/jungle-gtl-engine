#pragma once

#include "EngineAPI.h"
#include <cmath>

struct ENGINE_API FVector2
{
	float X;
	float Y;

	FVector2()
		: X(0.0f), Y(0.0f)
	{
	}

	FVector2(float InX, float InY)
		: X(InX), Y(InY)
	{
	}

	FVector2 operator-(const FVector2& Other) const
	{
		return FVector2(X - Other.X, Y - Other.Y);
	}

	FVector2 operator+(const FVector2& Other) const
	{
		return FVector2(X + Other.X, Y + Other.Y);
	}
};
