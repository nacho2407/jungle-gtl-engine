#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <immintrin.h>
#endif

/* Picking을 위한 SIMD를 따로 모아둔 코드입니다. 기존 SIMD와는 계산 양상이 달라 파일을 분리합니다. */

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

struct FOBBSIMDContext
{
	__m256 CenterX;
	__m256 CenterY;
	__m256 CenterZ;

	__m256 ExtentX;
	__m256 ExtentY;
	__m256 ExtentZ;

	__m256 R00, R01, R02;
	__m256 R10, R11, R12;
	__m256 R20, R21, R22;

	__m256 AbsR00, AbsR01, AbsR02;
	__m256 AbsR10, AbsR11, AbsR12;
	__m256 AbsR20, AbsR21, AbsR22;
};

struct FCollisionUtilsSIMD
{
	static FRaySIMDContext MakeRayContext(const FVector& Origin, const FVector& Direction);
	static FOBBSIMDContext MakeOBBContext(const struct FOBB& OBB);

	static int32 IntersectOBBAABB8(
		const FOBBSIMDContext& Context,
		const float* MinX, const float* MinY, const float* MinZ,
		const float* MaxX, const float* MaxY, const float* MaxZ);

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