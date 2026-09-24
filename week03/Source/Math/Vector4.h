#pragma once

#include "Matrix.h"

struct FVector4
{
	FVector4(float InX = 0.f, float InY = 0.f, float InZ = 0.f, float InW = 0.f)
		: x(InX), y(InY), z(InZ), w(InW) {
	};
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
	float w = 0.f;

	float Dot(const FVector4& Other);
	float Length();
	float Length3();

	FVector4 operator*(const FMatrix& Matrix)
	{
		float sum[4] = { 0,0,0,0 };
		for (int col = 0; col < 4;++col)
		{
			sum[col] += x * Matrix[0][col];
			sum[col] += y * Matrix[1][col];
			sum[col] += z * Matrix[2][col];
			sum[col] += w * Matrix[3][col];
		}
		return FVector4(sum[0], sum[1], sum[2], sum[3]);
	}
	FVector4 operator /(const float factor)
	{
		if (factor == 0.f)
		{
			return FVector4(0.f, 0.f, 0.f, 0.f);
		}
		return FVector4(x / factor, y / factor, z / factor, w / factor);
	}
	FVector4 operator* (const float factor)
	{
		return FVector4(x * factor, y * factor, z * factor, w * factor);
	}
};