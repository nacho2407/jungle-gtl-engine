#include "Engine/Render/Culling/ConvexVolume.h"
#include "Engine/Core/EngineTypes.h"

void FConvexVolume::UpdateFromMatrix(const FMatrix& InViewProjectionMatrix)
{
	const FMatrix& M = InViewProjectionMatrix;

	// Left
	Planes[0] = { M.M[0][3] + M.M[0][0], M.M[1][3] + M.M[1][0], M.M[2][3] + M.M[2][0], M.M[3][3] + M.M[3][0] };
	// Right
	Planes[1] = { M.M[0][3] - M.M[0][0], M.M[1][3] - M.M[1][0], M.M[2][3] - M.M[2][0], M.M[3][3] - M.M[3][0] };

	// Top
	Planes[2] = { M.M[0][3] - M.M[0][1], M.M[1][3] - M.M[1][1], M.M[2][3] - M.M[2][1], M.M[3][3] - M.M[3][1] };
	// Bottom
	Planes[3] = { M.M[0][3] + M.M[0][1], M.M[1][3] + M.M[1][1], M.M[2][3] + M.M[2][1], M.M[3][3] + M.M[3][1] };

	// Near
	Planes[4] = { M.M[0][2], M.M[1][2], M.M[2][2], M.M[3][2] };
	// Far
	Planes[5] = { M.M[0][3] - M.M[0][2], M.M[1][3] - M.M[1][2], M.M[2][3] - M.M[2][2], M.M[3][3] - M.M[3][2] };

	for (auto& P : Planes) {
		P.Normalize();
	}
}

bool FConvexVolume::IntersectAABB(const FBoundingBox& Box) const
{
	for (const auto& P : Planes) {
		FVector4 PVertex = Box.Min;
		if (P.X >= 0) PVertex.X = Box.Max.X;
		if (P.Y >= 0) PVertex.Y = Box.Max.Y;
		if (P.Z >= 0) PVertex.Z = Box.Max.Z;

		if (P.Dot(PVertex) < 0)
			return false;
	}
	return true;
}

bool FConvexVolume::ContainsAABB(const FBoundingBox& Box) const
{
	// Test negative vertex (NVertex) — the corner farthest in the negative
	// direction of each plane normal. If NVertex is inside all planes,
	// the entire AABB is fully contained.
	for (const auto& P : Planes) {
		FVector4 NVertex = Box.Max;
		if (P.X >= 0) NVertex.X = Box.Min.X;
		if (P.Y >= 0) NVertex.Y = Box.Min.Y;
		if (P.Z >= 0) NVertex.Z = Box.Min.Z;

		if (P.Dot(NVertex) < 0)
			return false;
	}
	return true;
}

EAABBFrustumClassify FConvexVolume::ClassifyAABB(const FBoundingBox& Box) const
{
	bool bContained = true;

	for (const auto& P : Planes)
	{
		FVector4 PVertex = Box.Min;
		if (P.X >= 0) PVertex.X = Box.Max.X;
		if (P.Y >= 0) PVertex.Y = Box.Max.Y;
		if (P.Z >= 0) PVertex.Z = Box.Max.Z;

		if (P.Dot(PVertex) < 0.0f)
			return EAABBFrustumClassify::Outside;

		FVector4 NVertex = Box.Max;
		if (P.X >= 0) NVertex.X = Box.Min.X;
		if (P.Y >= 0) NVertex.Y = Box.Min.Y;
		if (P.Z >= 0) NVertex.Z = Box.Min.Z;

		if (P.Dot(NVertex) < 0.0f)
			bContained = false;
	}

	return bContained ? EAABBFrustumClassify::Contains
	                  : EAABBFrustumClassify::Intersects;
}
