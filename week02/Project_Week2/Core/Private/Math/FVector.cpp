#include "Math/FVector.h"

FVector::FVector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

float FVector::Dot(const FVector& rhs)
{
	return x * rhs.x + y * rhs.y + z * rhs.z;
}

FVector FVector::Cross(const FVector& rhs)
{
	return FVector(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
}

float FVector::Length() const
{
	return sqrt(x * x + y * y + z * z);
}

FVector FVector::Normalize()
{
	float length = Length();
	return FVector(x /= length, y /= length, z /= length);
}

FVector FVector::operator+(const float rhs)
{
	return FVector(x + rhs, y + rhs, z + rhs);
}

FVector FVector::operator-(const float rhs)
{
	return FVector(x - rhs, y - rhs, z - rhs);
}

FVector FVector::operator*(const float rhs)
{
	return FVector(x * rhs, y * rhs, z * rhs);
}

FVector FVector::operator/(const float rhs)
{
	return FVector(x / rhs, y * rhs, z * rhs);
}

FVector FVector::operator-(const FVector rhs)
{
	return FVector(x - rhs.x, y - rhs.y, z - rhs.z);
}