#include "Collision/RayUtils.h"
#include "Component/PrimitiveComponent.h"
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <cstdint>

bool FRayUtils::CheckRayAABB(const FRay& Ray, const FVector& AABBMin, const FVector& AABBMax)
{
	float tMin = -INFINITY;
	float tMax = INFINITY;

	const float* origin = &Ray.Origin.X;
	const float* dir = &Ray.Direction.X;
	const float* minB = &AABBMin.X;
	const float* maxB = &AABBMax.X;

	for (int i = 0; i < 3; ++i)
	{
		float invDir = 1.0f / dir[i];
		float t1 = (minB[i] - origin[i]) * invDir;
		float t2 = (maxB[i] - origin[i]) * invDir;

		if (t1 > t2) std::swap(t1, t2);

		tMin = std::max(tMin, t1);
		tMax = std::min(tMax, t2);

		if (tMin > tMax) return false;
	}

	return tMax >= 0;
}

bool FRayUtils::IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir,
	const FVector& V0, const FVector& V1, const FVector& V2, float& OutT)
{
	FVector edge1 = V1 - V0;
	FVector edge2 = V2 - V0;
	FVector pvec = RayDir.Cross(edge2);
	float det = edge1.Dot(pvec);

	if (std::abs(det) < 0.0001f) return false;

	float invDet = 1.0f / det;
	FVector tvec = RayOrigin - V0;
	float u = tvec.Dot(pvec) * invDet;

	if (u < 0.0f || u > 1.0f) return false;

	FVector qvec = tvec.Cross(edge1);
	float v = RayDir.Dot(qvec) * invDet;

	if (v < 0.0f || u + v > 1.0f) return false;

	OutT = edge2.Dot(qvec) * invDet;
	return OutT > 0.0f;
}

bool FRayUtils::RaycastTriangles(
	const FRay& WorldRay,
	const FMatrix& WorldMatrix,
	const void* PositionData,
	uint32 PositionStride,
	const TArray<uint32>& Indices,
	FHitResult& OutHitResult)
{
	if (!PositionData || Indices.empty()) return false;

	FMatrix invWorld = WorldMatrix.GetInverse();
	FVector localOrigin = invWorld.TransformPositionWithW(WorldRay.Origin);
	FVector localDir = invWorld.TransformVector(WorldRay.Direction);
	localDir.Normalize();

	bool bHit = false;
	float closestT = FLT_MAX;
	int hitFaceIndex = -1;

	const uint8* basePtr = static_cast<const uint8*>(PositionData);

	for (size_t i = 0; i + 2 < Indices.size(); i += 3)
	{
		const FVector& v0 = *reinterpret_cast<const FVector*>(basePtr + Indices[i] * PositionStride);
		const FVector& v1 = *reinterpret_cast<const FVector*>(basePtr + Indices[i + 1] * PositionStride);
		const FVector& v2 = *reinterpret_cast<const FVector*>(basePtr + Indices[i + 2] * PositionStride);

		float t = 0.0f;
		if (IntersectTriangle(localOrigin, localDir, v0, v1, v2, t))
		{
			if (t > 0.0f && t < closestT)
			{
				closestT = t;
				hitFaceIndex = static_cast<int>(i);
				bHit = true;
			}
		}
	}

	OutHitResult.bHit = bHit;
	if (bHit)
	{
		OutHitResult.FaceIndex = hitFaceIndex;
		FVector localHitPoint = localOrigin + localDir * closestT;
		FVector worldHitPoint = WorldMatrix.TransformPositionWithW(localHitPoint);
		OutHitResult.Distance = FVector::Distance(WorldRay.Origin, worldHitPoint);
	}

	return bHit;
}

bool FRayUtils::RaycastComponent(UPrimitiveComponent* Component, const FRay& Ray, FHitResult& OutHitResult)
{
	if (!Component || !Component->IsVisible())
		return false;

	Component->UpdateWorldAABB();

	FBoundingBox AABB = Component->GetWorldBoundingBox();
	if (!CheckRayAABB(Ray, AABB.Min, AABB.Max))
		return false;

	return Component->LineTraceComponent(Ray, OutHitResult);
}
