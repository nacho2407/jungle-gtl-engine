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
	FVector O = FVector(0.0f, 0.0f, 0.0f);
	FVector D = FVector(1.0f, 0.0f, 0.0f);
	FVector InvD = FVector(0.0f, 0.0f, 0.0f);
	int DirIsNeg[3] = { 0, 0, 0 };

	Ray() = default;

	Ray(const FVector& InO, const FVector& InD)
		: O(InO), D(InD)
	{
		constexpr float Huge = std::numeric_limits<float>::max();
		const float Eps = 1e-8f;

		InvD.X = (std::fabs(D.X) < Eps) ? Huge : 1.0f / D.X;
		InvD.Y = (std::fabs(D.Y) < Eps) ? Huge : 1.0f / D.Y;
		InvD.Z = (std::fabs(D.Z) < Eps) ? Huge : 1.0f / D.Z;

		DirIsNeg[0] = InvD.X < 0.0f;
		DirIsNeg[1] = InvD.Y < 0.0f;
		DirIsNeg[2] = InvD.Z < 0.0f;
	}
};

struct FAABB
{
	FVector PMin;
	FVector PMax;

	FAABB()
	{
		constexpr float MaxF = std::numeric_limits<float>::max();
		PMin = FVector(MaxF, MaxF, MaxF);
		PMax = FVector(-MaxF, -MaxF, -MaxF);
	}

	FAABB(const FVector& InMin, const FVector& InMax)
		: PMin(InMin), PMax(InMax)
	{
	}

	void Expand(const FAABB& b)
	{
		PMin = VecMin(PMin, b.PMin);
		PMax = VecMax(PMax, b.PMax);
	}

	void Expand(const FVector& p)
	{
		PMin = VecMin(PMin, p);
		PMax = VecMax(PMax, p);
	}

	FVector Centroid() const
	{
		return (PMin + PMax) * 0.5f;
	}

	float SurfaceArea() const
	{
		const FVector e = PMax - PMin;
		return 2.0f * (e.X * e.Y + e.X * e.Z + e.Y * e.Z);
	}

	int MaxExtentAxis() const
	{
		const FVector e = PMax - PMin;
		if (e.X > e.Y && e.X > e.Z) return 0;
		if (e.Y > e.Z) return 1;
		return 2;
	}

	bool Intersect(const Ray& ray, float tMax) const
	{
		float tmin = 0.0f;
		float tmax = tMax;

		for (int a = 0; a < 3; ++a)
		{
			float t0 = (GetAxis(PMin, a) - GetAxis(ray.O, a)) * GetAxis(ray.InvD, a);
			float t1 = (GetAxis(PMax, a) - GetAxis(ray.O, a)) * GetAxis(ray.InvD, a);

			if (GetAxis(ray.InvD, a) < 0.0f)
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

struct FPrimRef
{
	FAABB Bounds;
	FVector Centroid = FVector(0.0f, 0.0f, 0.0f);
	UPrimitiveComponent* Primitive = nullptr;
};

struct BuildNode
{
	FAABB Bounds;
	BuildNode* Left = nullptr;
	BuildNode* Right = nullptr;
	int FirstPrimOffset = 0;
	int PrimCount = 0;
	int SplitAxis = 0;

	bool IsLeaf() const { return PrimCount > 0; }
};

struct LinearNode
{
	FAABB Bounds;

	union
	{
		int PrimitivesOffset;
		int SecondChildOffset;
	};

	uint16 PrimCount = 0;
	uint16 Axis = 0;
};

struct FBucket
{
	int32   Count = 0;
	FAABB  Bounds;
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
	TArray<FPrimRef> PrimitiveRefs;

	void DestroyNode(BuildNode* Node);
	BuildNode* BuildRecursive(int32 Start, int32 End);
	void QueryFrustumRecursive(const BuildNode* Node, const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void QueryRayRecursive(const BuildNode* Node, const Ray& InRay, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const;
};
