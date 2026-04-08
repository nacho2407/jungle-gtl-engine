#pragma once

#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Math/Matrix.h"

struct FRay
{
	FVector Origin;
	FVector Direction;
	FVector InvD; // 1/Direction, 평행 케이스는 IEEE 754 ±infinity로 자동 처리

	constexpr FRay() : Origin(), Direction(), InvD() {}

	FRay(const FVector& InOrigin, const FVector& InDirection)
		: Origin(InOrigin)
	{
		SetDirection(InDirection);
	}

	// Direction이 바뀔 때 무조건 InvD를 재계산하도록 강제하는 함수
	void SetDirection(const FVector& NewDirection)
	{
		Direction = NewDirection;

		// 0으로 나누면 IEEE 754에 의해 ±infinity가 되어 slab 테스트에서 평행 케이스를 자동 처리
		InvD.X = 1.0f / Direction.X;
		InvD.Y = 1.0f / Direction.Y;
		InvD.Z = 1.0f / Direction.Z;
	}

	static FRay BuildRay(int32 MouseX, int32 MouseY, const FMatrix& ViewProjection, float ViewportWidth,
		float ViewportHeight)
	{
		if (ViewportWidth <= 0 || ViewportHeight <= 0)
		{
			return FRay{};
		}

		const float NDCX =
			(2.0f * static_cast<float>(MouseX) / static_cast<float>(ViewportWidth) - 1.0f);
		const float NDCY =
			1.0f - (2.0f * static_cast<float>(MouseY) / static_cast<float>(ViewportHeight));

		const FVector NearPointNDC(NDCX, NDCY, 1.0f);
		const FVector FarPointNDC(NDCX, NDCY, 0.0f);

		const FMatrix InvViewProjection = ViewProjection.GetInverse();

		const FVector NearWorld = InvViewProjection.TransformPosition(NearPointNDC);
		const FVector FarWorld = InvViewProjection.TransformPosition(FarPointNDC);

		const FVector Direction = (FarWorld - NearWorld).GetSafeNormal();

		return FRay{ NearWorld, Direction };
	}
};
