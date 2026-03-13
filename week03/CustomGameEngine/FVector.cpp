#include "FVector.h"
#include <stdexcept>

float FVector::Dot(const FVector& Other) const
{
	return x * Other.x + y * Other.y + z * Other.z;
}

FVector FVector::Cross(const FVector& Other) const {
  return FVector(y * Other.z - z * Other.y, z * Other.x - x * Other.z,
                 x * Other.y - y * Other.x);
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

float FVector::IsNearlyEqual(const FVector& Other, float Epsilon) const
{
  return (std::abs(x - Other.x) < Epsilon) &&
         (std::abs(y - Other.y) < Epsilon) && (std::abs(z - Other.z) < Epsilon);
}

FVector Cross(const FVector& a, const FVector& b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}
