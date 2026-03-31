#include "Matrix.h"
#include "Quat.h"
#include "Rotator.h"
#include "Utils.h"
#include <iostream>

const FMatrix FMatrix::Identity(1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1);

FMatrix FMatrix::operator+(const FMatrix& Other) const {
	FMatrix ret;

	for (int i = 0; i < 16; i++)
	{
		ret.Data[i] = Data[i] + Other.Data[i];
	}

	/*for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			ret.M[i][j] = M[i][j] + Other.M[i][j];
		}
	}*/

	return ret;
}

FMatrix FMatrix::operator-(const FMatrix& Other) const {
	FMatrix ret;

	for (int i = 0; i < 16; i++)
	{
		ret.Data[i] = Data[i] - Other.Data[i];
	}

	//for (int i = 0; i < 4; i++) {
	//	for (int j = 0; j < 4; j++) {
	//		ret.M[i][j] = M[i][j] - Other.M[i][j];
	//	}
	//}

	return ret;
}

//	Standard matrix multiplication (not optimized)
FMatrix FMatrix::operator*(const FMatrix& Other) const {
	FMatrix ret{};

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 4; k++) {
				ret.M[i][j] += M[i][k] * Other.M[k][j];
			}
		}
	}

	return ret;
}

//	Scalar division
FMatrix FMatrix::operator/(float Scalar) const {
	FMatrix ret;
	if (Scalar < 1e-4) {
		return ret;	// Zero matrix
	}

	const float inv = 1.0f / Scalar;

	for (int i = 0; i < 16; i++)
	{
		ret.Data[i] = Data[i] * inv;
	}

	return ret;
}

FMatrix FMatrix::operator+(float Scalar) const {
	FMatrix ret;

	for (int i = 0; i < 16; i++)
	{
		ret.Data[i] = Data[i] + Scalar;
	}

	return ret;
}

FMatrix FMatrix::operator-(float Scalar) const {
	FMatrix ret;

	for (int i = 0; i < 16; i++)
	{
		ret.Data[i] = Data[i] - Scalar;
	}

	return ret;
}

FMatrix FMatrix::operator*(float Scalar) const {
	FMatrix ret{};

	for (int i = 0; i < 16; i++)
	{
		ret.Data[i] = Data[i] * Scalar;
	}

	return ret;
}

FMatrix& FMatrix::operator+=(const FMatrix& Other) {
	*this = *this + Other;
	return *this;
}

FMatrix& FMatrix::operator-=(const FMatrix& Other) {
	*this = *this - Other;
	return *this;
}

FMatrix& FMatrix::operator*=(const FMatrix& Other) {
	*this = *this * Other;
	return *this;
}

FMatrix& FMatrix::operator/=(float Scalar) {
	*this = *this / Scalar;
	return *this;
}

FMatrix& FMatrix::operator+=(float Scalar) {
	*this = *this + Scalar;
	return *this;
}

FMatrix& FMatrix::operator-=(float Scalar) {
	*this = *this - Scalar;
	return *this;
}

FMatrix& FMatrix::operator*=(float Scalar) {
	*this = *this * Scalar;
	return *this;
}

FMatrix FMatrix::GetTransposed() const {
	FMatrix ret;

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			ret.M[j][i] = M[i][j];
		}
	}

	return ret;
}

FMatrix FMatrix::GetInverse() const {
	// Compute 2x2 sub-determinants (cofactors) needed for the 4x4 inverse
	float A2323 = M[2][2] * M[3][3] - M[2][3] * M[3][2];
	float A1323 = M[2][1] * M[3][3] - M[2][3] * M[3][1];
	float A1223 = M[2][1] * M[3][2] - M[2][2] * M[3][1];
	float A0323 = M[2][0] * M[3][3] - M[2][3] * M[3][0];
	float A0223 = M[2][0] * M[3][2] - M[2][2] * M[3][0];
	float A0123 = M[2][0] * M[3][1] - M[2][1] * M[3][0];
	float A2313 = M[1][2] * M[3][3] - M[1][3] * M[3][2];
	float A1313 = M[1][1] * M[3][3] - M[1][3] * M[3][1];
	float A1213 = M[1][1] * M[3][2] - M[1][2] * M[3][1];
	float A2312 = M[1][2] * M[2][3] - M[1][3] * M[2][2];
	float A1312 = M[1][1] * M[2][3] - M[1][3] * M[2][1];
	float A1212 = M[1][1] * M[2][2] - M[1][2] * M[2][1];
	float A0313 = M[1][0] * M[3][3] - M[1][3] * M[3][0];
	float A0213 = M[1][0] * M[3][2] - M[1][2] * M[3][0];
	float A0312 = M[1][0] * M[2][3] - M[1][3] * M[2][0];
	float A0212 = M[1][0] * M[2][2] - M[1][2] * M[2][0];
	float A0113 = M[1][0] * M[3][1] - M[1][1] * M[3][0];
	float A0112 = M[1][0] * M[2][1] - M[1][1] * M[2][0];

	// Determinant - if 0, matrix is singular (no inverse exists)
	float det = M[0][0] * (M[1][1] * A2323 - M[1][2] * A1323 + M[1][3] * A1223)
		- M[0][1] * (M[1][0] * A2323 - M[1][2] * A0323 + M[1][3] * A0223)
		+ M[0][2] * (M[1][0] * A1323 - M[1][1] * A0323 + M[1][3] * A0123)
		- M[0][3] * (M[1][0] * A1223 - M[1][1] * A0223 + M[1][2] * A0123);

	if (std::fabsf(det) < 1e-6f) {
		// Matrix is singular, inverse does not exist
		FMatrix ret;
		return ret;
	}

	float invDet = 1.0f / det;

	FMatrix ret;
	ret.M[0][0] = invDet * (M[1][1] * A2323 - M[1][2] * A1323 + M[1][3] * A1223);
	ret.M[0][1] = -invDet * (M[0][1] * A2323 - M[0][2] * A1323 + M[0][3] * A1223);
	ret.M[0][2] = invDet * (M[0][1] * A2313 - M[0][2] * A1313 + M[0][3] * A1213);
	ret.M[0][3] = -invDet * (M[0][1] * A2312 - M[0][2] * A1312 + M[0][3] * A1212);
	ret.M[1][0] = -invDet * (M[1][0] * A2323 - M[1][2] * A0323 + M[1][3] * A0223);
	ret.M[1][1] = invDet * (M[0][0] * A2323 - M[0][2] * A0323 + M[0][3] * A0223);
	ret.M[1][2] = -invDet * (M[0][0] * A2313 - M[0][2] * A0313 + M[0][3] * A0213);
	ret.M[1][3] = invDet * (M[0][0] * A2312 - M[0][2] * A0312 + M[0][3] * A0212);
	ret.M[2][0] = invDet * (M[1][0] * A1323 - M[1][1] * A0323 + M[1][3] * A0123);
	ret.M[2][1] = -invDet * (M[0][0] * A1323 - M[0][1] * A0323 + M[0][3] * A0123);
	ret.M[2][2] = invDet * (M[0][0] * A1313 - M[0][1] * A0313 + M[0][3] * A0113);
	ret.M[2][3] = -invDet * (M[0][0] * A1312 - M[0][1] * A0312 + M[0][3] * A0112);
	ret.M[3][0] = -invDet * (M[1][0] * A1223 - M[1][1] * A0223 + M[1][2] * A0123);
	ret.M[3][1] = invDet * (M[0][0] * A1223 - M[0][1] * A0223 + M[0][2] * A0123);
	ret.M[3][2] = -invDet * (M[0][0] * A1213 - M[0][1] * A0213 + M[0][2] * A0113);
	ret.M[3][3] = invDet * (M[0][0] * A1212 - M[0][1] * A0212 + M[0][2] * A0112);

	return ret;
}

FMatrix FMatrix::GetInverseFast() const {
	// Transpose the 3x3 rotation block
	FMatrix ret;
	ret.M[0][0] = M[0][0]; ret.M[0][1] = M[1][0]; ret.M[0][2] = M[2][0];
	ret.M[1][0] = M[0][1]; ret.M[1][1] = M[1][1]; ret.M[1][2] = M[2][1];
	ret.M[2][0] = M[0][2]; ret.M[2][1] = M[1][2]; ret.M[2][2] = M[2][2];

	// Negate the translation
	ret.M[3][0] = -(M[3][0] * ret.M[0][0] + M[3][1] * ret.M[1][0] + M[3][2] * ret.M[2][0]);
	ret.M[3][1] = -(M[3][0] * ret.M[0][1] + M[3][1] * ret.M[1][1] + M[3][2] * ret.M[2][1]);
	ret.M[3][2] = -(M[3][0] * ret.M[0][2] + M[3][1] * ret.M[1][2] + M[3][2] * ret.M[2][2]);
	ret.M[3][3] = 1.0f;

	return ret;
}

bool FMatrix::Equals(const FMatrix& Other) const {

	for (int i = 0; i < 16; i++)
	{
		if (std::abs(Data[i] - Other.Data[i]) > 1e-4) {
			return false;
		}
	}

	return true;
}

bool FMatrix::IsIdentity() const
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			float expected = (i == j) ? 1.0f : 0.0f;
			if (std::fabsf(M[i][j] - expected) > 1e-4f) return false;
		}
	}
	return true;
}

FMatrix FMatrix::MakeTranslationMatrix(const FVector& Location)
{
	FMatrix ret = FMatrix::Identity;
	ret.M[3][0] = Location.X;
	ret.M[3][1] = Location.Y;
	ret.M[3][2] = Location.Z;
	return ret;
}

FMatrix FMatrix::MakeScaleMatrix(const FVector& Scale)
{
	FMatrix ret = FMatrix::Identity;
	ret.M[0][0] = Scale.X;
	ret.M[1][1] = Scale.Y;
	ret.M[2][2] = Scale.Z;
	return ret;
}

FMatrix FMatrix::MakeRotationEuler(const FVector& Rotation)
{
	float degreeToRad = 3.1415926535f / 180.0f;

	// 언리얼 기준: X:Roll, Y:Pitch, Z:Yaw
	float roll = Rotation.X * degreeToRad;
	float pitch = Rotation.Y * degreeToRad;
	float yaw = Rotation.Z * degreeToRad;

	return MakeRotationX(roll) * MakeRotationY(pitch) * MakeRotationZ(yaw);
}

FMatrix FMatrix::MakeRotationX(float Angle)
{
	FMatrix ret = FMatrix::Identity;
	float s = sinf(Angle); float c = cosf(Angle);
	ret.M[1][1] = c;  ret.M[1][2] = s;
	ret.M[2][1] = -s; ret.M[2][2] = c;
	return ret;
}

FMatrix FMatrix::MakeRotationY(float Angle)
{
	FMatrix ret = FMatrix::Identity;
	float s = sinf(Angle); float c = cosf(Angle);
	ret.M[0][0] = c;  ret.M[0][2] = -s;
	ret.M[2][0] = s; ret.M[2][2] = c;
	return ret;
}

FMatrix FMatrix::MakeRotationZ(float Angle)
{
	FMatrix ret = FMatrix::Identity;
	float s = sinf(Angle); float c = cosf(Angle);
	ret.M[0][0] = c;  ret.M[0][1] = s;
	ret.M[1][0] = -s; ret.M[1][1] = c;
	return ret;
}

FMatrix FMatrix::GetCancelRotationMatrix(const FMatrix& InMatrix)
{
	FMatrix ret = FMatrix::Identity;

	ret.M[0][0] = InMatrix.M[0][0];
	ret.M[0][1] = InMatrix.M[1][0];
	ret.M[0][2] = InMatrix.M[2][0];

	ret.M[1][0] = InMatrix.M[0][1];
	ret.M[1][1] = InMatrix.M[1][1];
	ret.M[1][2] = InMatrix.M[2][1];

	ret.M[2][0] = InMatrix.M[0][2];
	ret.M[2][1] = InMatrix.M[1][2];
	ret.M[2][2] = InMatrix.M[2][2];

	return ret;
}

FVector operator*(const FVector& vector, const FMatrix& matrix)
{
	FVector ret{};
	ret.X = vector.X * matrix.M[0][0] + vector.Y * matrix.M[1][0] + vector.Z * matrix.M[2][0] + matrix.M[3][0];
	ret.Y = vector.X * matrix.M[0][1] + vector.Y * matrix.M[1][1] + vector.Z * matrix.M[2][1] + matrix.M[3][1];
	ret.Z = vector.X * matrix.M[0][2] + vector.Y * matrix.M[1][2] + vector.Z * matrix.M[2][2] + matrix.M[3][2];
	return ret;
}

void FMatrix::Print() const
{
	for (int i = 0; i < 4; i++)
	{
		std::cout << "[ ";
		for (int j = 0; j < 4; j++)
		{
			std::cout << M[i][j];
			if (j < 3) std::cout << ", ";
		}
		std::cout << " ]\n";
	}
}

FVector FMatrix::TransformVector(const FVector& vector) const
{
	return FVector(
		vector.X * M[0][0] + vector.Y * M[1][0] + vector.Z * M[2][0],
		vector.X * M[0][1] + vector.Y * M[1][1] + vector.Z * M[2][1],
		vector.X * M[0][2] + vector.Y * M[1][2] + vector.Z * M[2][2]
	);
}

FVector FMatrix::TransformPositionWithW(const FVector& Vector) const
{
	float X = Vector.X * M[0][0] + Vector.Y * M[1][0] + Vector.Z * M[2][0] + 1.0f * M[3][0];
	float Y = Vector.X * M[0][1] + Vector.Y * M[1][1] + Vector.Z * M[2][1] + 1.0f * M[3][1];
	float Z = Vector.X * M[0][2] + Vector.Y * M[1][2] + Vector.Z * M[2][2] + 1.0f * M[3][2];
	float W = Vector.X * M[0][3] + Vector.Y * M[1][3] + Vector.Z * M[2][3] + 1.0f * M[3][3];

	if (W != 0.0f && W != 1.0f)
	{
		float InvW = 1.0f / W;
		return FVector(X * InvW, Y * InvW, Z * InvW);
	}

	return FVector(X, Y, Z);
}

FVector FMatrix::GetEuler() const
{
	FVector Euler;
	const float Rad2Deg = 180.0f / 3.1415926535f;

	float sp = -M[0][2];
	sp = Clamp(sp, -1.0f, 1.0f);
	Euler.Y = std::asin(sp) * Rad2Deg;

	if (std::abs(sp) > 0.9999f)
	{
		Euler.X = 0.0f; // Roll 고정
		Euler.Z = std::atan2(-M[1][0], M[1][1]) * Rad2Deg;
	}
	else
	{
		Euler.Z = std::atan2(M[0][1], M[0][0]) * Rad2Deg;

		Euler.X = std::atan2(M[1][2], M[2][2]) * Rad2Deg;
	}

	return Euler;
}

FVector FMatrix::GetLocation() const
{
	// 4번째 행(인덱스 3)의 0, 1, 2번째 열이 각각 X, Y, Z 위치입니다.
	return FVector(M[3][0], M[3][1], M[3][2]);
}

FVector FMatrix::GetScale() const
{
	float ScaleX = std::sqrt(M[0][0] * M[0][0] + M[0][1] * M[0][1] + M[0][2] * M[0][2]);
	float ScaleY = std::sqrt(M[1][0] * M[1][0] + M[1][1] * M[1][1] + M[1][2] * M[1][2]);
	float ScaleZ = std::sqrt(M[2][0] * M[2][0] + M[2][1] * M[2][1] + M[2][2] * M[2][2]);

	return FVector(ScaleX, ScaleY, ScaleZ);
}

void FMatrix::SetAxes(const FVector& Right, const FVector& Up, const FVector& Forward)
{
	M[0][0] = Right.X;   M[0][1] = Right.Y;   M[0][2] = Right.Z;   M[0][3] = 0.0f;
	M[1][0] = Up.X;      M[1][1] = Up.Y;      M[1][2] = Up.Z;      M[1][3] = 0.0f;
	M[2][0] = Forward.X; M[2][1] = Forward.Y; M[2][2] = Forward.Z; M[2][3] = 0.0f;
	M[3][0] = 0.0f;      M[3][1] = 0.0f;      M[3][2] = 0.0f;      M[3][3] = 1.0f;
}

FMatrix FMatrix::MakeRotationAxis(const FVector& Axis, float Angle)
{
	FMatrix ret = FMatrix::Identity;
	float s = sinf(Angle); float c = cosf(Angle); float t = 1.0f - c;
	FVector a = Axis; a.Normalize();

	ret.M[0][0] = t * a.X * a.X + c;       ret.M[0][1] = t * a.X * a.Y + s * a.Z; ret.M[0][2] = t * a.X * a.Z - s * a.Y;
	ret.M[1][0] = t * a.X * a.Y - s * a.Z; ret.M[1][1] = t * a.Y * a.Y + c;       ret.M[1][2] = t * a.Y * a.Z + s * a.X;
	ret.M[2][0] = t * a.X * a.Z + s * a.Y; ret.M[2][1] = t * a.Y * a.Z - s * a.X; ret.M[2][2] = t * a.Z * a.Z + c;

	return ret;
}

FQuat FMatrix::ToQuat() const
{
	return FQuat::FromMatrix(*this);
}

FRotator FMatrix::ToRotator() const
{
	return FRotator(GetEuler());
}