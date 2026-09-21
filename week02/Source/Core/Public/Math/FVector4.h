#pragma once
#include "Math/FVector.h"

struct FVector4
{
	float X, Y, Z, W;

public:
	FVector4();
	FVector4(float InX, float InY, float InZ, float InW);
	FVector4(const FVector& InVector, float InW);

	static const FVector4 ZeroVector;
	static const FVector4 OneVector;

public:
	float Dot(const FVector4& Other) const;
	float Length() const;
	float LengthSquared() const;

	float Length3() const;
	float LengthSquared3() const;

public:
	FVector4 operator+(const FVector4& Rhs) const;
	FVector4 operator-(const FVector4& Rhs) const;
	FVector4 operator*(float Scalar) const;
	FVector4 operator/(float Scalar) const;
};