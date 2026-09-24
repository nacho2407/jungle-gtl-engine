#pragma once

#include <cmath>

class MathHelper
{
public:
	static constexpr float PI = 3.14159265358979323846f;

	static float Clamp(float value, float min, float max)
	{
		if (value < min) return min;
		if (value > max) return max;
		return value;
	}

	static float Lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	static float DegToRad(float degree)
	{
		return degree * (PI / 180.0f);
	}

	static float RadToDeg(float radian)
	{
		return radian * (180.0f / PI);
	}

	static float Atan2(float y, float x)
	{
		return std::atan2(y, x);
	}

	static float ClampAngle(float Angle, float Min, float Max)
	{
		return Clamp(NormalizeAngle(Angle), Min, Max);
	}

	static float NormalizeAngle(float Angle)
	{
		while (Angle > 360.f)
			Angle -= 360.f;
		while (Angle < -360.0f)
			Angle += 360.0f;
		if (Angle > 180.0f) Angle -= 360.0f;
		if (Angle <= -180.0f) Angle += 360.0f;
		return Angle;
	}

	static float Sqrt(float Value) { return std::sqrt(Value); }
};