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
#if defined(__AVX2__) 
	__m128 vTemp = _mm_dp_ps(_mm_set_ps(0.f, Z, Y, X), _mm_set_ps(0.f, Other.Z, Other.Y, Other.X), 0xff);
	return vTemp.m128_f32[0];
#elif defined(_XM_SSE_INTRINSICS_) || defined(__SSE__)
	__m128 vTemp2 = _mm_set_ps(0.f, Other.Z, Other.Y, Other.X);
	__m128 vTemp1 = _mm_set_ps(0.f, Z, Y, X);
	__m128 vTemp = _mm_mul_ps(vTemp1, vTemp2);
	vTemp = _mm_hadd_ps(vTemp, vTemp);
	vTemp = _mm_hadd_ps(vTemp, vTemp);
	return vTemp.m128_f32[0];
#else
	return X * Other.X + Y * Other.Y + Z * Other.Z;
#endif
}

FVector FVector::Cross(const FVector& Other) const {
#if defined(_XM_SSE_INTRINSICS_) || defined(__SSE__)
	__m128 vec0 = _mm_set_ps(0.f, Z, Y, X);
	__m128 vec1 = _mm_set_ps(0.f, Other.Z, Other.Y, Other.X);
	alignas(16) float vTemp[4];

	// for intel
	__m128 tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
	__m128 tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
	__m128 tmp2 = _mm_mul_ps(tmp0, vec1);
	__m128 tmp3 = _mm_mul_ps(tmp0, tmp1);
	__m128 tmp4 = _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
	_mm_storeu_ps(vTemp, _mm_sub_ps(tmp3, tmp4));
	// for amd
	//__m128 tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
	//__m128 tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
	//__m128 tmp2 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 1, 0, 2));
	//__m128 tmp3 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 0, 2, 1));
	//__m128 tmp4 = _mm_sub_ps(_mm_mul_ps(tmp0, tmp1), _mm_mul_ps(tmp2, tmp3));
	//_mm_store_ps(vTemp, tmp4);
	return FVector(vTemp[0], vTemp[1], vTemp[2]);
#else
	return FVector{
		Y * Other.Z - Z * Other.Y,
		Z * Other.X - X * Other.Z,
		X * Other.Y - Y * Other.X
	};
#endif
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
#if defined(__AVX2__) 
	__m128 vTemp = _mm_dp_ps(_mm_set_ps(W, Z, Y, X), _mm_set_ps(Other.W, Other.Z, Other.Y, Other.X), 0xff);
	return vTemp.m128_f32[0];
#elif defined(_XM_SSE_INTRINSICS_) || defined(__SSE__)
	__m128 vTemp2 = _mm_set_ps(Other.W, Other.Z, Other.Y, Other.X);
	__m128 vTemp1 = _mm_set_ps(W, Z, Y, X);
	__m128 vTemp = _mm_mul_ps(vTemp1, vTemp2);
	vTemp = _mm_hadd_ps(vTemp, vTemp);
	vTemp = _mm_hadd_ps(vTemp, vTemp);
	return vTemp.m128_f32[0];
#else
	return X * Other.X + Y * Other.Y + Z * Other.Z + W * Other.W;
#endif
}

FVector4 FVector4::Cross(const FVector4& Other) const {
#if defined(_XM_SSE_INTRINSICS_) || defined(__SSE__)
	__m128 vec0 = _mm_set_ps(W, Z, Y, X);
	__m128 vec1 = _mm_set_ps(Other.W, Other.Z, Other.Y, Other.X);
	alignas(16) float vTemp[4];

	// for intel
	__m128 tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
	__m128 tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
	__m128 tmp2 = _mm_mul_ps(tmp0, vec1);
	__m128 tmp3 = _mm_mul_ps(tmp0, tmp1);
	__m128 tmp4 = _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
	_mm_storeu_ps(vTemp, _mm_sub_ps(tmp3, tmp4));
	// for amd
	//__m128 tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
	//__m128 tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
	//__m128 tmp2 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 1, 0, 2));
	//__m128 tmp3 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 0, 2, 1));
	//__m128 tmp4 = _mm_sub_ps(_mm_mul_ps(tmp0, tmp1), _mm_mul_ps(tmp2, tmp3));
	//_mm_store_ps(vTemp, tmp4);
	return FVector4(vTemp[0], vTemp[1], vTemp[2], 0.0f);
#else 
	FVector4 ret;
	ret.X = (Y * Other.Z) - (Z * Other.Y);
	ret.Y = (Z * Other.X) - (X * Other.Z);
	ret.Z = (X * Other.Y) - (Y * Other.X);
	ret.W = 0.0f;  // direction vector, not a point
	return ret;
#endif
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
