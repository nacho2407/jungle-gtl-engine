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

float FVector::DotProduct(const FVector& Other) const {
	return X * Other.X + Y * Other.Y + Z * Other.Z;
}

FVector FVector::CrossProduct(const FVector& Other) const {
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

float FVector::LengthSquared() const {
	return X * X + Y * Y + Z * Z;
}

void FVector::Set(float InX, float InY, float InZ) {
	X = InX;
	Y = InY;
	Z = InZ;
}

bool FVector::IsNearlyZero(float Epsilon) const {
	return std::fabs(X) <= Epsilon
		&& std::fabs(Y) <= Epsilon
		&& std::fabs(Z) <= Epsilon;
}

bool FVector::Equals(const FVector& Other, float Epsilon) const {
	return std::fabs(X - Other.X) <= Epsilon
		&& std::fabs(Y - Other.Y) <= Epsilon
		&& std::fabs(Z - Other.Z) <= Epsilon;
}

void FVector::NormalizeSafe(float Epsilon) {
	const float LenSq = LengthSquared();
	if (LenSq <= Epsilon * Epsilon) {
		return;
	}

	const float InvLen = 1.0f / std::sqrtf(LenSq);
	X *= InvLen;
	Y *= InvLen;
	Z *= InvLen;
}

FVector FVector::GetSafeNormal(float Epsilon) const {
	FVector Copy = *this;
	Copy.NormalizeSafe(Epsilon);
	return Copy;
}

float FVector::MaxComponent() const {
	return std::fmax(X, std::fmax(Y, Z));
}

float FVector::MinComponent() const {
	return std::fmin(X, std::fmin(Y, Z));
}

FVector FVector::Lerp(const FVector& A, const FVector& B, float Alpha) {
	return A + (B - A) * Alpha;
}

FVector FVector::Reflect(const FVector& Direction, const FVector& Normal) {
	const FVector SafeNormal = Normal.GetSafeNormal();
	return Direction - SafeNormal * (2.0f * Direction.DotProduct(SafeNormal));
}

FVector FVector::Project(const FVector& Vector, const FVector& OnNormal) {
	const float Denom = OnNormal.DotProduct(OnNormal);
	if (std::fabs(Denom) <= 1e-6f) {
		return FVector();
	}
	return OnNormal * (Vector.DotProduct(OnNormal) / Denom);
}

FVector FVector::ZeroVector() {
	return FVector(0.0f, 0.0f, 0.0f);
}

FVector FVector::OneVector() {
	return FVector(1.0f, 1.0f, 1.0f);
}

FVector FVector::UpVector() {
	return FVector(0.0f, 1.0f, 0.0f);
}

FVector FVector::RightVector() {
	return FVector(1.0f, 0.0f, 0.0f);
}

FVector FVector::ForwardVector() {
	return FVector(0.0f, 0.0f, 1.0f);
}

#pragma endregion

#pragma region __FVECTOR4__

float FVector4::Length() const {
	return std::sqrtf(X * X + Y * Y + Z * Z + W * W);
}

void FVector4::Normalize() {
	auto length = Length();
	if (std::abs(length) < 1e-6f) {
		return;
	}
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

float FVector4::LengthSquared() const {
	return X * X + Y * Y + Z * Z + W * W;
}

void FVector4::Set(float InX, float InY, float InZ, float InW) {
	X = InX;
	Y = InY;
	Z = InZ;
	W = InW;
}

bool FVector4::IsNearlyZero(float Epsilon) const {
	return std::fabs(X) <= Epsilon
		&& std::fabs(Y) <= Epsilon
		&& std::fabs(Z) <= Epsilon
		&& std::fabs(W) <= Epsilon;
}

bool FVector4::Equals(const FVector4& Other, float Epsilon) const {
	return std::fabs(X - Other.X) <= Epsilon
		&& std::fabs(Y - Other.Y) <= Epsilon
		&& std::fabs(Z - Other.Z) <= Epsilon
		&& std::fabs(W - Other.W) <= Epsilon;
}

void FVector4::NormalizeSafe(float Epsilon) {
	const float LenSq = LengthSquared();
	if (LenSq <= Epsilon * Epsilon) {
		return;
	}

	const float InvLen = 1.0f / std::sqrtf(LenSq);
	X *= InvLen;
	Y *= InvLen;
	Z *= InvLen;
	W *= InvLen;
}

FVector4 FVector4::GetSafeNormal(float Epsilon) const {
	FVector4 Copy = *this;
	Copy.NormalizeSafe(Epsilon);
	return Copy;
}

FVector4 FVector4::Lerp(const FVector4& A, const FVector4& B, float Alpha) {
	return A + (B - A) * Alpha;
}


#pragma endregion

#pragma region __FVECTOR2__

float FVector2::Length() const {
	return std::sqrtf(X * X + Y * Y);
}

void FVector2::Normalize() {
	auto length = Length();
	if (std::abs(length) < 1e-6f) {
		return;
	}
	X /= length;
	Y /= length;
}

FVector2 FVector2::Normalized() const {
	FVector2 copy = *this;
	copy.Normalize();
	return copy;
}

float FVector2::DotProduct(const FVector2& Other) const {
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

float FVector2::LengthSquared() const {
	return X * X + Y * Y;
}

void FVector2::Set(float InX, float InY) {
	X = InX;
	Y = InY;
}

bool FVector2::IsNearlyZero(float Epsilon) const {
	return std::fabs(X) <= Epsilon
		&& std::fabs(Y) <= Epsilon;
}

bool FVector2::Equals(const FVector2& Other, float Epsilon) const {
	return std::fabs(X - Other.X) <= Epsilon
		&& std::fabs(Y - Other.Y) <= Epsilon;
}

void FVector2::NormalizeSafe(float Epsilon) {
	const float LenSq = LengthSquared();
	if (LenSq <= Epsilon * Epsilon) {
		return;
	}

	const float InvLen = 1.0f / std::sqrtf(LenSq);
	X *= InvLen;
	Y *= InvLen;
}

FVector2 FVector2::GetSafeNormal(float Epsilon) const {
	FVector2 Copy = *this;
	Copy.NormalizeSafe(Epsilon);
	return Copy;
}

float FVector2::Distance(const FVector2& V1, const FVector2& V2) {
	return std::sqrtf(DistSquared(V1, V2));
}

float FVector2::DistSquared(const FVector2& V1, const FVector2& V2) {
	const float DX = V1.X - V2.X;
	const float DY = V1.Y - V2.Y;
	return DX * DX + DY * DY;
}

FVector2 FVector2::Lerp(const FVector2& A, const FVector2& B, float Alpha) {
	return A + (B - A) * Alpha;
}

FVector2 FVector2::ZeroVector() {
	return FVector2(0.0f, 0.0f);
}

FVector2 FVector2::OneVector() {
	return FVector2(1.0f, 1.0f);
}


#pragma endregion