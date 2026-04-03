#pragma once

#include "CoreMinimal.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <cstdint>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

constexpr int NUM_BUCKETS = 12;

class UPrimitiveComponent;
class FFrustum;

static inline FVector VecMin(const FVector& A, const FVector& B)
{
	return FVector(
		std::min(A.X, B.X),
		std::min(A.Y, B.Y),
		std::min(A.Z, B.Z)
	);
}

static inline FVector VecMax(const FVector& A, const FVector& B)
{
	return FVector(
		std::max(A.X, B.X),
		std::max(A.Y, B.Y),
		std::max(A.Z, B.Z)
	);
}

static inline float GetAxis(const FVector& V, int Axis)
{
	return (Axis == 0) ? V.X : (Axis == 1 ? V.Y : V.Z);
}

struct Ray
{
	FVector o = FVector(0.0f, 0.0f, 0.0f);
	FVector d = FVector(1.0f, 0.0f, 0.0f);
	FVector invD = FVector(0.0f, 0.0f, 0.0f);
	int dirIsNeg[3] = { 0, 0, 0 };

	Ray() = default;

	Ray(const FVector& InO, const FVector& InD)
		: o(InO), d(InD)
	{
		constexpr float Huge = std::numeric_limits<float>::max();
		const float Eps = 1e-8f;

		invD.X = (std::fabs(d.X) < Eps) ? Huge : 1.0f / d.X;
		invD.Y = (std::fabs(d.Y) < Eps) ? Huge : 1.0f / d.Y;
		invD.Z = (std::fabs(d.Z) < Eps) ? Huge : 1.0f / d.Z;

		dirIsNeg[0] = invD.X < 0.0f;
		dirIsNeg[1] = invD.Y < 0.0f;
		dirIsNeg[2] = invD.Z < 0.0f;
	}
};

struct AABB
{
	FVector pMin;
	FVector pMax;

	AABB()
	{
		constexpr float MaxF = std::numeric_limits<float>::max();
		pMin = FVector(MaxF, MaxF, MaxF);
		pMax = FVector(-MaxF, -MaxF, -MaxF);
	}

	AABB(const FVector& InMin, const FVector& InMax)
		: pMin(InMin), pMax(InMax)
	{
	}

	void expand(const AABB& b)
	{
		pMin = VecMin(pMin, b.pMin);
		pMax = VecMax(pMax, b.pMax);
	}

	void expand(const FVector& p)
	{
		pMin = VecMin(pMin, p);
		pMax = VecMax(pMax, p);
	}

	FVector centroid() const
	{
		return (pMin + pMax) * 0.5f;
	}

	float surfaceArea() const
	{
		const FVector e = pMax - pMin;
		return 2.0f * (e.X * e.Y + e.X * e.Z + e.Y * e.Z);
	}

	int maxExtentAxis() const
	{
		const FVector e = pMax - pMin;
		if (e.X > e.Y && e.X > e.Z) return 0;
		if (e.Y > e.Z) return 1;
		return 2;
	}

	bool intersect(const Ray& ray, float tMax) const
	{
		float tmin = 0.0f;
		float tmax = tMax;

		for (int a = 0; a < 3; ++a)
		{
			float t0 = (GetAxis(pMin, a) - GetAxis(ray.o, a)) * GetAxis(ray.invD, a);
			float t1 = (GetAxis(pMax, a) - GetAxis(ray.o, a)) * GetAxis(ray.invD, a);

			if (GetAxis(ray.invD, a) < 0.0f)
			{
				std::swap(t0, t1);
			}

			tmin = (t0 > tmin) ? t0 : tmin;
			tmax = (t1 < tmax) ? t1 : tmax;

			if (tmax < tmin)
			{
				return false;
			}
		}

		return true;
	}
};

struct PrimRef
{
	AABB bounds;
	FVector centroid = FVector(0.0f, 0.0f, 0.0f);
	UPrimitiveComponent* primitive = nullptr;
};

struct BuildNode
{
	AABB bounds;
	BuildNode* left = nullptr;
	BuildNode* right = nullptr;
	int firstPrimOffset = 0;
	int primCount = 0;
	int splitAxis = 0;

	bool isLeaf() const { return primCount > 0; }
};

struct LinearNode
{
	AABB bounds;

	union
	{
		int primitivesOffset;
		int secondChildOffset;
	};

	uint16 primCount = 0;
	uint16 axis = 0;
};

struct FBucket
{
	int   Count = 0;
	AABB  Bounds;
};

class BVH
{
public:
	BVH() = default;
	~BVH();

	void Reset();
	void Build(const TArray<UPrimitiveComponent*>& InPrimitives);
	void QueryFrustum(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void QueryRay(const Ray& InRay, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	bool IsEmpty() const { return Root == nullptr; }

private:
	static constexpr int32 MaxPrimitivesPerLeaf = 8;

	BuildNode* Root = nullptr;
	TArray<PrimRef> PrimitiveRefs;

	void DestroyNode(BuildNode* Node);
	BuildNode* BuildRecursive(int32 Start, int32 End);
	void QueryFrustumRecursive(const BuildNode* Node, const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void QueryRayRecursive(const BuildNode* Node, const Ray& InRay, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	int32 ComputeSAH(int32 Start, int32 End, int32 Axis);
};
