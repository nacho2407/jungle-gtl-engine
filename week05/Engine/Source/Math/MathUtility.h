#pragma once

#include "EngineAPI.h"

struct ENGINE_API FMath
{
	static constexpr float PI           = 3.14159265358979323846f;
	static constexpr float InvPI        = 0.31830988618379067154f;
	static constexpr float HalfPI       = 1.57079632679489661923f;
	static constexpr float TwoPi        = 6.28318530717958647692f;

	static constexpr float SmallNumber      = 1.e-8f;
	static constexpr float KindaSmallNumber = 1.e-4f;

	static constexpr float DegreesToRadians(float Degrees)
	{
		return Degrees * (PI / 180.0f);
	}

	static constexpr float RadiansToDegrees(float Radians)
	{
		return Radians * (180.0f / PI);
	}

	static float Max(float A, float B)
	{
		return A < B ? B : A;
	}

	static float Min(float A, float B)
	{
		return A < B ? A : B;
	}

	template <typename T>
	static T Clamp(const T& Value, const T& Min, const T& Max)
	{
		if (Value < Min) return Min;
		if (Value > Max) return Max;
		return Value;
	}
};
