#pragma once

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

};