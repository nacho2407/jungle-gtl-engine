#include "Vector.h"

#pragma region __FVECTOR__

float FVector::Length() const {
	return std::sqrtf(X * X + Y * Y + Z * Z);
}

void FVector::Normalize() {
	auto length = Length();

	if (std::abs(length) < 1e-4) {
		return;
	}
	X = X / length;
	Y = Y / length;
	Z = Z / length;
}

FVector FVector::Normalized() const {
	FVector copy = *this;
	copy.Normalize();
	return copy;
}

float FVector::Dot(const FVector& Other) const {
	return X * Other.X + Y * Other.Y + Z * Other.Z;
}

FVector FVector::Cross(const FVector& Other) const {
	FVector ret;

	ret.X = (Y * Other.Z) - (Z * Other.Y);
	ret.Y = (Z * Other.X) - (X * Other.Z);
	ret.Z = (X * Other.Y) - (Y * Other.X);

	return ret;
}

float FVector::Distance(const FVector& V1, const FVector& V2) {
	return std::sqrt(DistSquared(V1, V2));
}

float FVector::DistSquared(const FVector& V1, const FVector& V2) {
	float DX = V1.X - V2.X;
	float DY = V1.Y - V2.Y;
	float DZ = V1.Z - V2.Z;
	return (DX * DX) + (DY * DY) + (DZ * DZ);
}

FVector FVector::operator+(const FVector& Other) const {
	FVector ret;
	ret.X = X + Other.X;
	ret.Y = Y + Other.Y;
	ret.Z = Z + Other.Z;
	return ret;
}

FVector FVector::operator-(const FVector& Other) const {
	FVector ret;
	ret.X = X - Other.X;
	ret.Y = Y - Other.Y;
	ret.Z = Z - Other.Z;
	return ret;
}

FVector FVector::operator+(float Scalar) const {
	FVector ret;
	ret.X = X + Scalar;
	ret.Y = Y + Scalar;
	ret.Z = Z + Scalar;
	return ret;
}

FVector FVector::operator-(float Scalar) const {
	FVector ret;
	ret.X = X - Scalar;
	ret.Y = Y - Scalar;
	ret.Z = Z - Scalar;
	return ret;
}

FVector FVector::operator*(float Scalar) const {
	FVector ret;
	ret.X = X * Scalar;
	ret.Y = Y * Scalar;
	ret.Z = Z * Scalar;
	return ret;
}

FVector FVector::operator/(float Scalar) const {
	FVector ret;
	ret.X = X / Scalar;
	ret.Y = Y / Scalar;
	ret.Z = Z / Scalar;
	return ret;
}

FVector& FVector::operator+=(const FVector& Other) {
	*this = *this + Other;
	return *this;
}

FVector& FVector::operator-=(const FVector& Other) {
	*this = *this - Other;
	return *this;
}

FVector& FVector::operator+=(float Scalar) {
	*this = *this + Scalar;
	return *this;
}

FVector& FVector::operator-=(float Scalar) {
	*this = *this - Scalar;
	return *this;
}

FVector& FVector::operator*=(float Scalar) {
	*this = *this * Scalar;
	return *this;
}

FVector& FVector::operator/=(float Scalar) {
	*this = *this / Scalar;
	return *this;
}

#pragma endregion

#pragma region __FVECTOR4__

float FVector4::Length() const {
	return std::sqrtf(X * X + Y * Y + Z * Z + W * W);
}

void FVector4::Normalize() {
	auto length = Length();
	X /= length;
	Y /= length;
	Z /= length;
	W /= length;
}

FVector4 FVector4::Normalized() const {
	FVector4 copy = *this;
	copy.Normalize();
	return copy;
}

float FVector4::Dot(const FVector4& Other) const {
	return X * Other.X + Y * Other.Y + Z * Other.Z + W * Other.W;
}

FVector4 FVector4::Cross(const FVector4& Other) const {
	FVector4 ret;
	ret.X = (Y * Other.Z) - (Z * Other.Y);
	ret.Y = (Z * Other.X) - (X * Other.Z);
	ret.Z = (X * Other.Y) - (Y * Other.X);
	ret.W = 0.0f;  // direction vector, not a point
	return ret;
}

FVector4 FVector4::operator+(const FVector4& Other) const {
	FVector4 ret;
	ret.X = X + Other.X;
	ret.Y = Y + Other.Y;
	ret.Z = Z + Other.Z;
	ret.W = W + Other.W;
	return ret;
}

FVector4 FVector4::operator-(const FVector4& Other) const {
	FVector4 ret;
	ret.X = X - Other.X;
	ret.Y = Y - Other.Y;
	ret.Z = Z - Other.Z;
	ret.W = W - Other.W;
	return ret;
}

FVector4 FVector4::operator+(float Scalar) const {
	FVector4 ret;
	ret.X = X + Scalar;
	ret.Y = Y + Scalar;
	ret.Z = Z + Scalar;
	ret.W = W + Scalar;
	return ret;
}

FVector4 FVector4::operator-(float Scalar) const {
	FVector4 ret;
	ret.X = X - Scalar;
	ret.Y = Y - Scalar;
	ret.Z = Z - Scalar;
	ret.W = W - Scalar;
	return ret;
}

FVector4 FVector4::operator*(float Scalar) const {
	FVector4 ret;
	ret.X = X * Scalar;
	ret.Y = Y * Scalar;
	ret.Z = Z * Scalar;
	ret.W = W * Scalar;
	return ret;
}

FVector4 FVector4::operator/(float Scalar) const {
	FVector4 ret;
	ret.X = X / Scalar;
	ret.Y = Y / Scalar;
	ret.Z = Z / Scalar;
	ret.W = W / Scalar;
	return ret;
}

FVector4& FVector4::operator+=(const FVector4& Other) {
	*this = *this + Other;
	return *this;
}

FVector4& FVector4::operator-=(const FVector4& Other) {
	*this = *this - Other;
	return *this;
}

FVector4& FVector4::operator+=(float Scalar) {
	*this = *this + Scalar;
	return *this;
}

FVector4& FVector4::operator-=(float Scalar) {
	*this = *this - Scalar;
	return *this;
}

FVector4& FVector4::operator*=(float Scalar) {
	*this = *this * Scalar;
	return *this;
}

FVector4& FVector4::operator/=(float Scalar) {
	*this = *this / Scalar;
	return *this;
}

#pragma endregion

#pragma region __FVECTOR2__

float FVector2::Length() const {
	return std::sqrtf(X * X + Y * Y);
}

void FVector2::Normalize() {
	auto length = Length();
	X /= length;
	Y /= length;
}

FVector2 FVector2::Normalized() const {
	FVector2 copy = *this;
	copy.Normalize();
	return copy;
}

float FVector2::Dot(const FVector2& Other) const {
	return X * Other.X + Y * Other.Y;
}

FVector2 FVector2::operator+(const FVector2& Other) const {
	FVector2 ret;
	ret.X = X + Other.X;
	ret.Y = Y + Other.Y;
	return ret;
}

FVector2 FVector2::operator-(const FVector2& Other) const {
	FVector2 ret;
	ret.X = X - Other.X;
	ret.Y = Y - Other.Y;
	return ret;
}

FVector2 FVector2::operator+(float Scalar) const {
	FVector2 ret;
	ret.X = X + Scalar;
	ret.Y = Y + Scalar;
	return ret;
}

FVector2 FVector2::operator-(float Scalar) const {
	FVector2 ret;
	ret.X = X - Scalar;
	ret.Y = Y - Scalar;
	return ret;
}

FVector2 FVector2::operator*(float Scalar) const {
	FVector2 ret;
	ret.X = X * Scalar;
	ret.Y = Y * Scalar;
	return ret;
}

FVector2 FVector2::operator/(float Scalar) const {
	FVector2 ret;
	ret.X = X / Scalar;
	ret.Y = Y / Scalar;
	return ret;
}

FVector2& FVector2::operator+=(const FVector2& Other) {
	*this = *this + Other;
	return *this;
}

FVector2& FVector2::operator-=(const FVector2& Other) {
	*this = *this - Other;
	return *this;
}

FVector2& FVector2::operator+=(float Scalar) {
	*this = *this + Scalar;
	return *this;
}

FVector2& FVector2::operator-=(float Scalar) {
	*this = *this - Scalar;
	return *this;
}

FVector2& FVector2::operator*=(float Scalar) {
	*this = *this * Scalar;
	return *this;
}

FVector2& FVector2::operator/=(float Scalar) {
	*this = *this / Scalar;
	return *this;
}

#pragma endregion