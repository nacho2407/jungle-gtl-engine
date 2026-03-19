#include <cassert>

#include "MathHelper.h"
#include "Vector.h"
#include "Matrix.h"

FVector::FVector(float InX, float InY, float InZ) : x(InX), y(InY), z(InZ)
{
}

FVector::~FVector()
{
}

float FVector::Dot(const FVector& Other) const
{
	return x * Other.x + y * Other.y + z * Other.z;
}

FVector FVector::Cross(const FVector& Other) const
{
	return FVector(y * Other.z - z * Other.y, z * Other.x - x * Other.z, x * Other.y - y * Other.x);
}

FVector FVector::operator+(const FVector& Other) const
{
	return FVector(x + Other.x, y + Other.y, z + Other.z);
}

FVector FVector::operator-(const FVector& Other) const
{
	return FVector(x - Other.x, y - Other.y, z - Other.z);
}

FVector FVector::operator*(const float s) const
{
	return FVector(x * s, y * s, z * s);
}

FVector FVector::operator/(const float s) const
{
	if (std::abs(s) < MathHelper::Epsilon)
	{
		assert(s != 0.0f && "Division by zero in FVector::operator/");
		return FVector::Zero();
	}
	return FVector(x / s, y / s, z / s);
}

FVector FVector::Normalize() const
{
	float SquareSum = x * x + y * y + z * z;
	float Denominator = MathHelper::Sqrt(SquareSum);

	if (std::abs(Denominator) < MathHelper::Epsilon)
		return FVector::Zero();
	Denominator = 1.0f / Denominator;

	return FVector(x * Denominator, y * Denominator, z * Denominator);
}

float FVector::Length() const
{
	return MathHelper::Sqrt(x * x + y * y + z * z);
}

bool FVector::IsNearlyEqual(const FVector& Other) const
{
	return (std::abs(x - Other.x) < MathHelper::Epsilon)
		&& (std::abs(y - Other.y) < MathHelper::Epsilon)
		&& (std::abs(z - Other.z) < MathHelper::Epsilon);
}

bool FVector::operator==(const FVector& Other) const
{
	return IsNearlyEqual(Other);
}

FVector FVector::Zero()
{
	return FVector(0.0f, 0.0f, 0.0f);
}

FVector FVector::Up()
{
	return FVector(0.0f, 0.0f, 1.0f);
}

FVector FVector::Right()
{
	return FVector(0.0f, 1.0f, 0.0f);
}

FVector FVector::Forward()
{
	return FVector(1.0f, 0.0f, 0.0f);
}