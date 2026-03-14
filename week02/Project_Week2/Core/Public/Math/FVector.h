#pragma once
#include <cmath>

struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0);
	float Dot(const FVector& rhs);
	FVector Cross(const FVector& rhs);
	float Length() const;
	FVector Normalize();

	FVector operator+(const float rhs);
	FVector operator-(const float rhs);
	FVector operator*(const float rhs);
	FVector operator/(const float rhs);
	FVector operator-(const FVector rhs);
};