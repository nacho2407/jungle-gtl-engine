#include "Math/FVector.h"

#include <cmath>

const FVector FVector::ZeroVector = FVector(0.0f, 0.0f, 0.0f);
const FVector FVector::OneVector = FVector(1.0f, 1.0f, 1.0f);
const FVector FVector::UpVector = FVector(0.0f, 1.0f, 0.0f);
const FVector FVector::RightVector = FVector(1.0f, 0.0f, 0.0f);
const FVector FVector::ForwardVector = FVector(0.0f, 0.0f, 1.0f);
// DirectX는 왼손 좌표계 아님? 그러면 UpVector는...Z축 아님??
// forward는 x축? right는 y축?

FVector::FVector() : X(0.0f), Y(0.0f), Z(0.0f) { }

FVector::FVector(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}

float FVector::Dot(const FVector& Rhs) const
{
	return X * Rhs.X + Y * Rhs.Y + Z * Rhs.Z;
}

FVector FVector::Cross(const FVector& Rhs) const
{
	return FVector(
		Y * Rhs.Z - Z * Rhs.Y,
		Z * Rhs.X - X * Rhs.Z,
		X * Rhs.Y - Y * Rhs.X
	);
}

float FVector::LengthSquared() const
{
	return X * X + Y * Y + Z * Z;
}

float FVector::Length() const
{
	return std::sqrt(LengthSquared());
}

void FVector::Normalize()
{
	const float Len = Length();
	if (Len > 0.000001f)
	{
		X /= Len;
		Y /= Len;
		Z /= Len;
	}
}

FVector FVector::operator+(const FVector& Rhs) const
{
	return FVector(X + Rhs.X, Y + Rhs.Y, Z + Rhs.Z);
}

FVector FVector::operator-(const FVector& Rhs) const
{
	return FVector(X - Rhs.X, Y - Rhs.Y, Z - Rhs.Z);
}

FVector FVector::operator*(float Scalar) const
{
	return FVector(X * Scalar, Y * Scalar, Z * Scalar);
}

FVector FVector::operator/(float Scalar) const
{
	return FVector(X / Scalar, Y / Scalar, Z / Scalar);
}

FVector FVector::GetNormalized() const
{
	const float Len = Length();
	if (Len > 0.000001f)
	{
		return FVector::ZeroVector;
	}
	return FVector::ZeroVector;
}

FVector& FVector::operator+=(const FVector& Rhs)
{
	X += Rhs.X;
	Y += Rhs.Y;
	Z += Rhs.Z;
	return *this;
}

FVector& FVector::operator-=(const FVector& Rhs)
{
	X -= Rhs.X;
	Y -= Rhs.Y;
	Z -= Rhs.Z;
	return *this;
}

FVector& FVector::operator*=(float Scalar)
{
	X *= Scalar;
	Y *= Scalar;
	Z *= Scalar;
	return *this;
}

FVector& FVector::operator/=(float Scalar)
{
	X /= Scalar;
	Y /= Scalar;
	Z /= Scalar;
	return *this;
}

bool FVector::operator==(const FVector& Rhs) const
{
	return X == Rhs.X && Y == Rhs.Y && Z == Rhs.Z;
}

bool FVector::operator!=(const FVector& Rhs) const
{
	return !(*this == Rhs);
}