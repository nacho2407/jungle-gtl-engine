#include "Collision/IntersectionUtils.h"

#include <cmath>

namespace
{
	struct FOrientedBox
	{
		FVector Center;
		FVector Axis[3];
		FVector HalfExtents;
	};

	FVector GetNormalizedAxis(const FMatrix& WorldMatrix, const FVector& LocalAxis)
	{
		FVector Axis = WorldMatrix.TransformVector(LocalAxis);
		const float Length = Axis.Length();
		if (Length > 1e-6f)
		{
			Axis /= Length;
		}
		else
		{
			Axis = LocalAxis;
		}
		return Axis;
	}

	FOrientedBox MakeOBB(const FMatrix& WorldMatrix, const FVector& HalfExtents)
	{
		FOrientedBox Box = {};
		Box.Center = WorldMatrix.GetLocation();
		Box.Axis[0] = GetNormalizedAxis(WorldMatrix, FVector(1.0f, 0.0f, 0.0f));
		Box.Axis[1] = GetNormalizedAxis(WorldMatrix, FVector(0.0f, 1.0f, 0.0f));
		Box.Axis[2] = GetNormalizedAxis(WorldMatrix, FVector(0.0f, 0.0f, 1.0f));
		Box.HalfExtents = HalfExtents;
		return Box;
	}

	FOrientedBox MakeAABBAsOBB(const FBoundingBox& AABB)
	{
		FOrientedBox Box = {};
		Box.Center = AABB.GetCenter();
		Box.Axis[0] = FVector(1.0f, 0.0f, 0.0f);
		Box.Axis[1] = FVector(0.0f, 1.0f, 0.0f);
		Box.Axis[2] = FVector(0.0f, 0.0f, 1.0f);
		Box.HalfExtents = AABB.GetExtent();
		return Box;
	}
}

bool FIntersectionUtils::IntersectOBBAndAABB(const FMatrix& OBBWorldMatrix, const FVector& OBBHalfExtents, const FBoundingBox& AABB)
{
	if (!AABB.IsValid())
	{
		return false;
	}

	constexpr float Epsilon = 1e-5f;

	const FOrientedBox BoxA = MakeOBB(OBBWorldMatrix, OBBHalfExtents);
	const FOrientedBox BoxB = MakeAABBAsOBB(AABB);

	float R[3][3] = {};
	float AbsR[3][3] = {};

	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			R[i][j] = BoxA.Axis[i].Dot(BoxB.Axis[j]);
			AbsR[i][j] = std::fabs(R[i][j]) + Epsilon;
		}
	}

	const FVector TranslationWorld = BoxB.Center - BoxA.Center;
	const float T[3] = {
		TranslationWorld.Dot(BoxA.Axis[0]),
		TranslationWorld.Dot(BoxA.Axis[1]),
		TranslationWorld.Dot(BoxA.Axis[2])
	};

	const float EA[3] = { BoxA.HalfExtents.X, BoxA.HalfExtents.Y, BoxA.HalfExtents.Z };
	const float EB[3] = { BoxB.HalfExtents.X, BoxB.HalfExtents.Y, BoxB.HalfExtents.Z };

	for (int32 i = 0; i < 3; ++i)
	{
		const float RA = EA[i];
		const float RB = EB[0] * AbsR[i][0] + EB[1] * AbsR[i][1] + EB[2] * AbsR[i][2];
		if (std::fabs(T[i]) > RA + RB)
		{
			return false;
		}
	}

	for (int32 j = 0; j < 3; ++j)
	{
		const float RA = EA[0] * AbsR[0][j] + EA[1] * AbsR[1][j] + EA[2] * AbsR[2][j];
		const float RB = EB[j];
		const float Projection = std::fabs(T[0] * R[0][j] + T[1] * R[1][j] + T[2] * R[2][j]);
		if (Projection > RA + RB)
		{
			return false;
		}
	}

	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			const float RA = EA[(i + 1) % 3] * AbsR[(i + 2) % 3][j] + EA[(i + 2) % 3] * AbsR[(i + 1) % 3][j];
			const float RB = EB[(j + 1) % 3] * AbsR[i][(j + 2) % 3] + EB[(j + 2) % 3] * AbsR[i][(j + 1) % 3];
			const float Projection = std::fabs(T[(i + 2) % 3] * R[(i + 1) % 3][j] - T[(i + 1) % 3] * R[(i + 2) % 3][j]);
			if (Projection > RA + RB)
			{
				return false;
			}
		}
	}

	return true;
}
