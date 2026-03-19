#include "Math/FVector4.h"
#include <cmath>

const FVector4 FVector4::ZeroVector = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
const FVector4 FVector4::OneVector = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

FVector4::FVector4() : X(0.0f), Y(0.0f), Z(0.0f), W(0.0f) {}
FVector4::FVector4(float InX, float InY, float InZ, float InW) 
	: X(InX), Y(InY), Z(InZ), W(InW) {}
FVector4::FVector4(const FVector& InVector, float InW)
	: X(InVector.X), Y(InVector.Y), Z(InVector.Z), W(InW) {}

float FVector4::Dot(const FVector4& Other) const
{
	return X * Other.X + Y * Other.Y + Z * Other.Z + W * Other.W;
}

float FVector4::LengthSquared() const
{
	return X * X + Y * Y + Z * Z + W * W;
}

float FVector4::Length() const
{
	return std::sqrt(LengthSquared());
}

float FVector4::LengthSquared3() const
{
	return X * X + Y * Y + Z * Z;
}

float FVector4::Length3() const
{
	return std::sqrt(LengthSquared3());
}

FVector4 FVector4::operator+(const FVector4& Rhs) const
{
	return FVector4(X + Rhs.X, Y + Rhs.Y, Z + Rhs.Z, W + Rhs.W);
}

FVector4 FVector4::operator-(const FVector4& Rhs) const
{
	return FVector4(X - Rhs.X, Y - Rhs.Y, Z - Rhs.Z, W - Rhs.W);
}

FVector4 FVector4::operator*(float Scalar) const
{
	return FVector4(X * Scalar, Y * Scalar, Z * Scalar, W * Scalar);
}

FVector4 FVector4::operator/(float Scalar) const
{
	return FVector4(X / Scalar, Y / Scalar, Z / Scalar, W / Scalar);
}