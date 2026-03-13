#include "FVector.h"
#include <stdexcept>

float FVector::Dot(const FVector& Other) const
{
	return x * Other.x + y * Other.y + z * Other.z;
}

FVector FVector::operator+(const FVector& Other) const
{
	return FVector(x + Other.x, y + Other.y, z + Other.z);
}

FVector  FVector::operator-(const FVector& Other) const
{
	return FVector(x - Other.x, y - Other.y, z - Other.z);
}

FVector  FVector::operator*(const float s) const
{
	return FVector(x * s, y * s, z * s);
}

FVector  FVector::operator/(const float s) const
{
	if (s == 0)
		throw std::invalid_argument("Division by zero condition!");
	return FVector(x / s, y / s, z / s);
}

FVector FVector::Normalize() const
{
	float SquareSum = x * x + y * y + z * z;
	float Denominator = sqrtf(SquareSum);

	return FVector(x / Denominator, y / Denominator, z / Denominator);
}

float FVector::Length() const
{
	return sqrtf(x * x + y * y + z * z);
}

inline float Dot(const FVector& a, const FVector& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

// 두 벡터의 함
inline FVector Add(const FVector& a, const FVector& b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

// 두 벡터의 차
inline FVector Sub(const FVector& a, const FVector& b)
{

	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

// 벡터 곱
inline FVector Mul(const FVector& v, float s)
{
	return { v.x * s, v.y * s, v.z * s };
}

// 벡터 나눗셈
inline FVector Div(const FVector& v, float s)
{
	if (s == 0)
		throw std::invalid_argument("Division by zero condition!");
	return { v.x / s, v.y / s, v.z / s };
}

FVector Cross(const FVector& a, const FVector& b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}
