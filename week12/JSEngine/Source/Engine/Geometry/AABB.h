#pragma once
#pragma once

#include "Engine/Math/Vector.h"

struct FRay;
struct FMatrix;

struct FAABB
{
	FVector Min;
	FVector Max;

	FAABB();
	FAABB(const FVector& InMin, const FVector& InMax);

	void Reset();
	bool IsValid() const;

	// 구를 움직이는 query는 중심점을 움직이고, target bounds를 radius만큼 늘려서 검사
	// 저장된 bounds는 바꾸면 안 되므로 확장된 복사본만 반환
	FAABB ExpandedBy(float Radius) const;
	FAABB ExpandedBy(const FVector& Extent) const;

	void Expand(const FVector& Point);
	void Merge(const FAABB& Other);

	inline FVector GetCenter() const
	{
		// Original:
		// return (Min + Max) * 0.5f;

		const XMVector Center =
			DirectX::XMVectorScale(DirectX::XMVectorAdd(Min.ToXMVector(), Max.ToXMVector()), 0.5f);
		return FVector(Center);
	}
	inline FVector GetExtent() const
	{
		// Original:
		// return (Max - Min) * 0.5f;

		const XMVector Extent = DirectX::XMVectorScale(
			DirectX::XMVectorSubtract(Max.ToXMVector(), Min.ToXMVector()), 0.5f);
		return FVector(Extent);
	}
	inline void GetVertices(FVector OutVertices[8]) const
	{
		OutVertices[0] = Min;
		OutVertices[1] = FVector(Min.X, Min.Y, Max.Z);
		OutVertices[2] = FVector(Min.X, Max.Y, Min.Z);
		OutVertices[3] = FVector(Min.X, Max.Y, Max.Z);
		OutVertices[4] = FVector(Max.X, Min.Y, Min.Z);
		OutVertices[5] = FVector(Max.X, Min.Y, Max.Z);
		OutVertices[6] = FVector(Max.X, Max.Y, Min.Z);
		OutVertices[7] = Max;
	}

	bool IntersectRay(const FRay& Ray, float& OutT) const;
	bool IntersectRay(const FRay& Ray, float& OutTMin, float& OutTMax) const;

	/**
	 * Start에서 End까지 이동하는 선분이
	 * AABB의 X/Y/Z 세 축 범위 안에
	 * 동시에 존재하는 구간이 있는지 검사하는 함수
	 */
	bool IntersectSegment(const FVector& Start, const FVector& End, float& OutTEnter, float& OutTExit) const;
	float SquaredDistanceToPoint(const FVector& Point) const;
	bool IntersectsSphere(const FVector& Center, float Radius) const;
	static FAABB TransformAABB(const FAABB& InLocalAABB, const FMatrix& InMatrix);
	void ExpandToInclude(const FAABB& Other);
	bool NearlyEqualAABB(const FAABB& Other) const;
	static bool NearlyEqualAABB(const FAABB& A, const FAABB& B);
};
