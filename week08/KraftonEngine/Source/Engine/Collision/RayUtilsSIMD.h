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
	__m256 ParallelXMask;
	__m256 ParallelYMask;
	__m256 ParallelZMask;
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

	static int32 IntersectTriangles8Precomputed(
		const FRaySIMDContext& Context,
		const float* V0X, const float* V0Y, const float* V0Z,
		const float* Edge1X, const float* Edge1Y, const float* Edge1Z,
		const float* Edge2X, const float* Edge2Y, const float* Edge2Z,
		float MaxDistance,
		float* OutTValues);
};
