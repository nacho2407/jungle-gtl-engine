#pragma once
#include "Math/Vector.h"
#include "Math/Matrix.h"

struct FQuadric
{
	// [ q0  q1  q2  q3 ]
	// [ q1  q4  q5  q6 ]
	// [ q2  q5  q7  q8 ]
	// [ q3  q6  q8  q9 ]
	float Q[10] = {};

	FQuadric() = default;

	static FMatrix ToMatrix(FQuadric Q)
	{
		return FMatrix(
			Q.Q[0], Q.Q[1], Q.Q[2], Q.Q[3],
			Q.Q[1], Q.Q[4], Q.Q[5], Q.Q[6],
			Q.Q[2], Q.Q[5], Q.Q[7], Q.Q[8],
			Q.Q[3], Q.Q[6], Q.Q[8], Q.Q[9]
		);
	}

	void AddPlane(const FVector& N, float D)
	{
		Q[0] += N.X * N.X;
		Q[1] += N.X * N.Y;
		Q[2] += N.X * N.Z;
		Q[3] += N.X * D;
		Q[4] += N.Y * N.Y;
		Q[5] += N.Y * N.Z;
		Q[6] += N.Y * D;
		Q[7] += N.Z * N.Z;
		Q[8] += N.Z * D;
		Q[9] += D * D;
	}

	void RemovePlane(const FVector& N, float D)
	{
		Q[0] -= N.X * N.X;
		Q[1] -= N.X * N.Y;
		Q[2] -= N.X * N.Z;
		Q[3] -= N.X * D;
		Q[4] -= N.Y * N.Y;
		Q[5] -= N.Y * N.Z;
		Q[6] -= N.Y * D;
		Q[7] -= N.Z * N.Z;
		Q[8] -= N.Z * D;
		Q[9] -= D * D;
	}

	float Evaluate(const FVector& V) const
	{
		const float X = V.X, Y = V.Y, Z = V.Z;
		return Q[0] * X * X + 2.f * Q[1] * X * Y + 2.f * Q[2] * X * Z + 2.f * Q[3] * X
			+ Q[4] * Y * Y + 2.f * Q[5] * Y * Z + 2.f * Q[6] * Y
			+ Q[7] * Z * Z + 2.f * Q[8] * Z
			+ Q[9];
	}

	FQuadric operator+(const FQuadric& Other) const
	{
		FQuadric Result;
		for (int i = 0; i < 10; ++i)
			Result.Q[i] = Q[i] + Other.Q[i];
		return Result;
	}

	static FVector FindOptimalPosition(
		const FQuadric& MergedQ,
		const FVector& VA,
		const FVector& VB,
		bool bFixA,
		bool bFixB)
	{
		if (bFixA) return VA;
		if (bFixB) return VB;

		FMatrix M(
			MergedQ.Q[0], MergedQ.Q[1], MergedQ.Q[2], 0.f,
			MergedQ.Q[1], MergedQ.Q[4], MergedQ.Q[5], 0.f,
			MergedQ.Q[2], MergedQ.Q[5], MergedQ.Q[7], 0.f,
			0.f, 0.f, 0.f, 1.f
		);

		if (!M.IsInvertible())
			return (VA + VB) * 0.5f;

		const FMatrix Inv = M.GetInverse();
		const FVector RHS(-MergedQ.Q[3], -MergedQ.Q[6], -MergedQ.Q[8]);
		return Inv.TransformVector(RHS);
	}
};