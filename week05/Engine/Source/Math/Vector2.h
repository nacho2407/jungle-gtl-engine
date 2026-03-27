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
};
