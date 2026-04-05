#pragma once
#include "Edge.h"
#include "Quadric.h"
#include "Vector.h"
#include "Renderer/Vertex.h"

struct FCollapse
{
	FEdge    Edge;
	FQuadric Quadric;
	FVector Position;
	float   Error = 0.f;
	float   Length = 0.f;
	bool    bFixA = false;
	bool    bFixB = false;
	int     Phase = 0;

	FCollapse() = default;

	bool operator<(const FCollapse& Other) const
	{
		if (Error != Other.Error)   return Error < Other.Error;
		if (Length != Other.Length) return Length < Other.Length;
		return Edge < Other.Edge;
	}

	bool operator==(const FCollapse& Other) const
	{
		return Edge == Other.Edge;
	}

	static FMatrix MakePlaneQuadric(float a, float b, float c, float d)
	{
		return FMatrix(
			a * a, a * b, a * c, a * d,
			a * b, b * b, b * c, b * d,
			a * c, b * c, c * c, c * d,
			a * d, b * d, c * d, d * d
		);
	}

	static float EvaluateQuadric(const FMatrix& Q, const FVector& P)
	{
		const float v[4] = { P.X, P.Y, P.Z, 1.0f };

		float Error = 0.0f;
		for (int r = 0; r < 4; ++r)
		{
			for (int c = 0; c < 4; ++c)
			{
				Error += v[r] * Q.M[r][c] * v[c];
			}
		}
		return Error;
	}

	static bool TryFindOptimalPosition(const FQuadric& Q, FVector& OutPos, float Tolerance = 1e-8f)
	{
		FMatrix M = FQuadric::ToMatrix(Q);

		M[0][3] = 0.0f;
		M[1][3] = 0.0f;
		M[2][3] = 0.0f;
		M[3][3] = 1.0f;

		if (!M.Inverse(Tolerance)) return false;

		OutPos = M.GetOrigin();
		return true;
	}

	static FVector FindOptimalPosition(const FQuadric& Quadric, const FVertex& VertexA, const FVertex& VertexB, bool InbFixA, bool InbFixB)
	{
		if (InbFixA) return VertexA.Position;
		if (InbFixB) return VertexB.Position;

		FVector P;

		if (TryFindOptimalPosition(Quadric, P)) return P;

		const FVector Mid(
			(VertexA.Position.X + VertexB.Position.X) * 0.5f,
			(VertexA.Position.Y + VertexB.Position.Y) * 0.5f,
			(VertexA.Position.Z + VertexB.Position.Z) * 0.5f
		);

		const float EA = EvaluateQuadric(FQuadric::ToMatrix(Quadric), VertexA.Position);
		const float EB = EvaluateQuadric(FQuadric::ToMatrix(Quadric), VertexB.Position);
		const float EM = EvaluateQuadric(FQuadric::ToMatrix(Quadric), Mid);

		if (EA <= EB && EA <= EM) return VertexA.Position;
		if (EB <= EA && EB <= EM) return VertexB.Position;
		return Mid;
	}
};