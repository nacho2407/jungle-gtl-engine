#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <immintrin.h>
#endif

struct FRaySIMDContext
{
	__m256 OriginX;
	__m256 OriginY;
	__m256 OriginZ;
	__m256 DirectionX;
	__m256 DirectionY;
	__m256 DirectionZ;
	__m256 InvDirectionX;
	__m256 InvDirectionY;
	__m256 InvDirectionZ;
};

struct FRayUtilsSIMD
{
	static FRaySIMDContext MakeRayContext(const FVector& Origin, const FVector& Direction);

	static int32 IntersectAABB8(
		const FRaySIMDContext& Context,
		const float* MinX, const float* MinY, const float* MinZ,
		const float* MaxX, const float* MaxY, const float* MaxZ,
		float MaxDistance,
		float* OutTMinValues);

	static int32 IntersectTriangles8(
		const FRaySIMDContext& Context,
		const float* V0X, const float* V0Y, const float* V0Z,
		const float* V1X, const float* V1Y, const float* V1Z,
		const float* V2X, const float* V2Y, const float* V2Z,
		float MaxDistance,
		float* OutTValues);
};
