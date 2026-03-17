#include "Vector.h"
#include "Matrix.h"
#include <stdexcept>

FVector::~FVector() {}

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

	if (Denominator == 0) return FVector(0.0f, 0.0f, 0.0f);

	return FVector(x / Denominator, y / Denominator, z / Denominator);
}

float FVector::Length() const
{
	return sqrtf(x * x + y * y + z * z);
}

bool FVector::IsNearlyEqual(const FVector& Other, float Epsilon) const
{
  return (std::abs(x - Other.x) < Epsilon) &&
         (std::abs(y - Other.y) < Epsilon) && (std::abs(z - Other.z) < Epsilon);
}

FVector FVector::Transform(const FMatrix& Mat)
{
	FVector vec;
	
	vec.x = x * Mat[0][0] + y * Mat[1][0] + z * Mat[2][0];
	vec.y = x * Mat[0][1] + y * Mat[1][1] + z * Mat[2][1];
	vec.y = x * Mat[0][2] + y * Mat[1][2] + z * Mat[2][2];

	return vec;
}

json::JSON FVector::Serialize()
{
	json::JSON j;
	j[0] = x;
	j[1] = y;
	j[2] = z;
	return j;
}

void FVector::Deserialize(json::JSON jsonObj)
{
	x = jsonObj[0].ToFloat();
	y = jsonObj[1].ToFloat();
	z = jsonObj[2].ToFloat();
}

FVector Cross(const FVector& a, const FVector& b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}
