#pragma once
#include <cmath>
#include "Vector.h"
#include "Utils.h"

struct FRotator;
struct FMatrix;

struct FQuat
{
	float X, Y, Z, W;

	FQuat() : X(0.0f), Y(0.0f), Z(0.0f), W(1.0f) {}
	FQuat(float InX, float InY, float InZ, float InW) : X(InX), Y(InY), Z(InZ), W(InW) {}

	// 축-각 생성 (Axis는 정규화, AngleRad는 라디안)
	static FQuat FromAxisAngle(const FVector& Axis, float AngleRad)
	{
		float Half = AngleRad * 0.5f;
		float S = sinf(Half);
		return FQuat(Axis.X * S, Axis.Y * S, Axis.Z * S, cosf(Half));
	}

	// 쿼터니언 곱 (회전 합성)
	FQuat operator*(const FQuat& Q) const
	{
		return FQuat(
			W * Q.X + X * Q.W + Y * Q.Z - Z * Q.Y,
			W * Q.Y - X * Q.Z + Y * Q.W + Z * Q.X,
			W * Q.Z + X * Q.Y - Y * Q.X + Z * Q.W,
			W * Q.W - X * Q.X - Y * Q.Y - Z * Q.Z
		);
	}

	FQuat& operator*=(const FQuat& Q)
	{
		*this = *this * Q;
		return *this;
	}

	float SizeSquared() const { return X * X + Y * Y + Z * Z + W * W; }
	float Size() const { return sqrtf(SizeSquared()); }

	void Normalize()
	{
		float S = Size();
		if (S > EPSILON)
		{
			float Inv = 1.0f / S;
			X *= Inv; Y *= Inv; Z *= Inv; W *= Inv;
		}
	}

	FQuat GetNormalized() const
	{
		FQuat Result = *this;
		Result.Normalize();
		return Result;
	}

	FQuat Inverse() const
	{
		// 단위 쿼터니언의 역 = 켤레
		return FQuat(-X, -Y, -Z, W);
	}

	// 벡터 회전: q * v * q^-1
	FVector RotateVector(const FVector& V) const
	{
		FQuat VQ(V.X, V.Y, V.Z, 0.0f);
		FQuat Result = *this * VQ * Inverse();
		return FVector(Result.X, Result.Y, Result.Z);
	}

	FVector GetForwardVector() const { return RotateVector(FVector(1.0f, 0.0f, 0.0f)); }
	FVector GetRightVector()   const { return RotateVector(FVector(0.0f, 1.0f, 0.0f)); }
	FVector GetUpVector()      const { return RotateVector(FVector(0.0f, 0.0f, 1.0f)); }

	// Spherical Linear Interpolation
	static FQuat Slerp(const FQuat& A, const FQuat& B, float Alpha)
	{
		float CosAngle = A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W;

		// 최단 경로 보장
		FQuat B2 = B;
		if (CosAngle < 0.0f)
		{
			B2 = FQuat(-B.X, -B.Y, -B.Z, -B.W);
			CosAngle = -CosAngle;
		}

		float S0, S1;
		if (CosAngle > 0.9999f)
		{
			// 거의 같은 방향 — 선형 보간 후 정규화
			S0 = 1.0f - Alpha;
			S1 = Alpha;
		}
		else
		{
			float Angle = acosf(CosAngle);
			float InvSin = 1.0f / sinf(Angle);
			S0 = sinf((1.0f - Alpha) * Angle) * InvSin;
			S1 = sinf(Alpha * Angle) * InvSin;
		}

		return FQuat(
			S0 * A.X + S1 * B2.X,
			S0 * A.Y + S1 * B2.Y,
			S0 * A.Z + S1 * B2.Z,
			S0 * A.W + S1 * B2.W
		).GetNormalized();
	}

	bool Equals(const FQuat& Other, float Tolerance = EPSILON) const
	{
		return fabsf(X - Other.X) < Tolerance
			&& fabsf(Y - Other.Y) < Tolerance
			&& fabsf(Z - Other.Z) < Tolerance
			&& fabsf(W - Other.W) < Tolerance;
	}

	static const FQuat Identity;

	// 변환 — 선언만, 구현은 Quat.cpp (순환 의존 방지)
	FRotator ToRotator() const;
	FMatrix ToMatrix() const;
	static FQuat FromRotator(const FRotator& Rot);
	static FQuat FromMatrix(const FMatrix& Mat);
};
