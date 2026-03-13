#pragma once
#include "../FMatrix.h"
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

	FVector4 operator*(const FMatrix4x4& Matrix)
	{
		int sum[4] = { 0,0,0,0 };
		for (int col = 0; col < 4;++col)
		{
			sum[col] += x * Matrix[0][col];
			sum[col] += y * Matrix[0][col];
			sum[col] += z * Matrix[0][col];
			sum[col] += w * Matrix[0][col];
		}
		return FVector4(sum[0], sum[1], sum[2], sum[3]);
	}
};