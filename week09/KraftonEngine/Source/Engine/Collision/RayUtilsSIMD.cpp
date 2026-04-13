#include "Collision/RayUtilsSIMD.h"

#include <cmath>

namespace
{
	inline float SafeInv(float Value)
	{
		return std::abs(Value) > 1e-8f ? 1.0f / Value : 1e30f;
	}

	inline __m256 MakeParallelMask(float Value)
	{
		return _mm256_castsi256_ps(_mm256_set1_epi32(std::abs(Value) <= 1e-8f ? -1 : 0));
	}
}

FRaySIMDContext FRayUtilsSIMD::MakeRayContext(const FVector& Origin, const FVector& Direction)
{
	// SISD 버전이라면 ray 원점/방향/invDirection/축별 parallel 여부를
	// float 변수 몇 개로 한 번 계산해 두고, 이후 각 AABB/triangle 테스트에서
	// 그 값을 반복 재사용하는 준비 단계에 해당합니다.
	FRaySIMDContext Context;
	Context.OriginX = _mm256_set1_ps(Origin.X);
	Context.OriginY = _mm256_set1_ps(Origin.Y);
	Context.OriginZ = _mm256_set1_ps(Origin.Z);
	Context.DirectionX = _mm256_set1_ps(Direction.X);
	Context.DirectionY = _mm256_set1_ps(Direction.Y);
	Context.DirectionZ = _mm256_set1_ps(Direction.Z);
	Context.InvDirectionX = _mm256_set1_ps(SafeInv(Direction.X));
	Context.InvDirectionY = _mm256_set1_ps(SafeInv(Direction.Y));
	Context.InvDirectionZ = _mm256_set1_ps(SafeInv(Direction.Z));
	Context.ParallelXMask = MakeParallelMask(Direction.X);
	Context.ParallelYMask = MakeParallelMask(Direction.Y);
	Context.ParallelZMask = MakeParallelMask(Direction.Z);
	return Context;
}

int32 FRayUtilsSIMD::IntersectAABB8(
	const FRaySIMDContext& Context,
	const float* MinX, const float* MinY, const float* MinZ,
	const float* MaxX, const float* MaxY, const float* MaxZ,
	float MaxDistance,
	float* OutTMinValues)
{
	// SISD 버전이라면 아래 8개 lane 각각에 대해
	// FRayUtils::IntersectRayAABB(ray, boxMin[i], boxMax[i], tMin, tMax)를
	// for문으로 한 번씩 호출하고, hit면 tMin을 저장하고 bit를 세우는 코드입니다.
	// Picking용 packet 데이터는 32-byte 정렬을 보장하므로 aligned load를 사용한다.
	// Ryzen 9 5900HX 같은 AVX2 타깃에서는 loadu 대비 불필요한 완화 비용을 줄일 수 있다.
	const __m256 MinXVec = _mm256_load_ps(MinX);
	const __m256 MinYVec = _mm256_load_ps(MinY);
	const __m256 MinZVec = _mm256_load_ps(MinZ);
	const __m256 MaxXVec = _mm256_load_ps(MaxX);
	const __m256 MaxYVec = _mm256_load_ps(MaxY);
	const __m256 MaxZVec = _mm256_load_ps(MaxZ);
	const __m256 NegInf = _mm256_set1_ps(-INFINITY);
	const __m256 PosInf = _mm256_set1_ps(INFINITY);
	const __m256 AllBits = _mm256_castsi256_ps(_mm256_set1_epi32(-1));

	const __m256 T1XRaw = _mm256_mul_ps(_mm256_sub_ps(MinXVec, Context.OriginX), Context.InvDirectionX);
	const __m256 T2XRaw = _mm256_mul_ps(_mm256_sub_ps(MaxXVec, Context.OriginX), Context.InvDirectionX);
	const __m256 T1YRaw = _mm256_mul_ps(_mm256_sub_ps(MinYVec, Context.OriginY), Context.InvDirectionY);
	const __m256 T2YRaw = _mm256_mul_ps(_mm256_sub_ps(MaxYVec, Context.OriginY), Context.InvDirectionY);
	const __m256 T1ZRaw = _mm256_mul_ps(_mm256_sub_ps(MinZVec, Context.OriginZ), Context.InvDirectionZ);
	const __m256 T2ZRaw = _mm256_mul_ps(_mm256_sub_ps(MaxZVec, Context.OriginZ), Context.InvDirectionZ);

	const __m256 T1X = _mm256_blendv_ps(T1XRaw, NegInf, Context.ParallelXMask);
	const __m256 T2X = _mm256_blendv_ps(T2XRaw, PosInf, Context.ParallelXMask);
	const __m256 T1Y = _mm256_blendv_ps(T1YRaw, NegInf, Context.ParallelYMask);
	const __m256 T2Y = _mm256_blendv_ps(T2YRaw, PosInf, Context.ParallelYMask);
	const __m256 T1Z = _mm256_blendv_ps(T1ZRaw, NegInf, Context.ParallelZMask);
	const __m256 T2Z = _mm256_blendv_ps(T2ZRaw, PosInf, Context.ParallelZMask);

	const __m256 TMin = _mm256_max_ps(
		_mm256_max_ps(_mm256_min_ps(T1X, T2X), _mm256_min_ps(T1Y, T2Y)),
		_mm256_min_ps(T1Z, T2Z));
	const __m256 TMax = _mm256_min_ps(
		_mm256_min_ps(_mm256_max_ps(T1X, T2X), _mm256_max_ps(T1Y, T2Y)),
		_mm256_max_ps(T1Z, T2Z));

	const __m256 XInside = _mm256_and_ps(
		_mm256_cmp_ps(Context.OriginX, MinXVec, _CMP_GE_OQ),
		_mm256_cmp_ps(Context.OriginX, MaxXVec, _CMP_LE_OQ));
	const __m256 YInside = _mm256_and_ps(
		_mm256_cmp_ps(Context.OriginY, MinYVec, _CMP_GE_OQ),
		_mm256_cmp_ps(Context.OriginY, MaxYVec, _CMP_LE_OQ));
	const __m256 ZInside = _mm256_and_ps(
		_mm256_cmp_ps(Context.OriginZ, MinZVec, _CMP_GE_OQ),
		_mm256_cmp_ps(Context.OriginZ, MaxZVec, _CMP_LE_OQ));

	const __m256 XParallelAccept = _mm256_or_ps(_mm256_xor_ps(Context.ParallelXMask, AllBits), XInside);
	const __m256 YParallelAccept = _mm256_or_ps(_mm256_xor_ps(Context.ParallelYMask, AllBits), YInside);
	const __m256 ZParallelAccept = _mm256_or_ps(_mm256_xor_ps(Context.ParallelZMask, AllBits), ZInside);

	const __m256 HitMask = _mm256_and_ps(
		_mm256_and_ps(XParallelAccept, _mm256_and_ps(YParallelAccept, ZParallelAccept)),
		_mm256_and_ps(
			_mm256_cmp_ps(TMin, TMax, _CMP_LE_OQ),
			_mm256_and_ps(
				_mm256_cmp_ps(TMax, _mm256_setzero_ps(), _CMP_GE_OQ),
				_mm256_cmp_ps(TMin, _mm256_set1_ps(MaxDistance), _CMP_LT_OQ))));

	if (OutTMinValues)
	{
		_mm256_storeu_ps(OutTMinValues, TMin);
	}

	return _mm256_movemask_ps(HitMask);
}

int32 FRayUtilsSIMD::IntersectTriangles8(
	const FRaySIMDContext& Context,
	const float* V0X, const float* V0Y, const float* V0Z,
	const float* V1X, const float* V1Y, const float* V1Z,
	const float* V2X, const float* V2Y, const float* V2Z,
	float MaxDistance,
	float* OutTValues)
{
	// SISD 버전이라면 8개 삼각형에 대해 순차적으로
	// Moller-Trumbore 교차 판정을 수행하고, 각 triangle의 t가
	// (0, MaxDistance) 범위에 들어오면 hit mask를 세우는 코드입니다.
	// 삼각형 packet 역시 SoA + 32-byte aligned layout으로 저장되므로 load_ps를 전제로 한다.
	const __m256 V0XVec = _mm256_load_ps(V0X);
	const __m256 V0YVec = _mm256_load_ps(V0Y);
	const __m256 V0ZVec = _mm256_load_ps(V0Z);
	const __m256 V1XVec = _mm256_load_ps(V1X);
	const __m256 V1YVec = _mm256_load_ps(V1Y);
	const __m256 V1ZVec = _mm256_load_ps(V1Z);
	const __m256 V2XVec = _mm256_load_ps(V2X);
	const __m256 V2YVec = _mm256_load_ps(V2Y);
	const __m256 V2ZVec = _mm256_load_ps(V2Z);

	const __m256 Edge1X = _mm256_sub_ps(V1XVec, V0XVec);
	const __m256 Edge1Y = _mm256_sub_ps(V1YVec, V0YVec);
	const __m256 Edge1Z = _mm256_sub_ps(V1ZVec, V0ZVec);
	const __m256 Edge2X = _mm256_sub_ps(V2XVec, V0XVec);
	const __m256 Edge2Y = _mm256_sub_ps(V2YVec, V0YVec);
	const __m256 Edge2Z = _mm256_sub_ps(V2ZVec, V0ZVec);

	const __m256 PVecX = _mm256_sub_ps(_mm256_mul_ps(Context.DirectionY, Edge2Z), _mm256_mul_ps(Context.DirectionZ, Edge2Y));
	const __m256 PVecY = _mm256_sub_ps(_mm256_mul_ps(Context.DirectionZ, Edge2X), _mm256_mul_ps(Context.DirectionX, Edge2Z));
	const __m256 PVecZ = _mm256_sub_ps(_mm256_mul_ps(Context.DirectionX, Edge2Y), _mm256_mul_ps(Context.DirectionY, Edge2X));

	const __m256 Det = _mm256_add_ps(
		_mm256_mul_ps(Edge1X, PVecX),
		_mm256_add_ps(_mm256_mul_ps(Edge1Y, PVecY), _mm256_mul_ps(Edge1Z, PVecZ)));
	const __m256 AbsDet = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), Det);
	const __m256 InvDet = _mm256_div_ps(_mm256_set1_ps(1.0f), Det);

	const __m256 TVecX = _mm256_sub_ps(Context.OriginX, V0XVec);
	const __m256 TVecY = _mm256_sub_ps(Context.OriginY, V0YVec);
	const __m256 TVecZ = _mm256_sub_ps(Context.OriginZ, V0ZVec);
	const __m256 U = _mm256_mul_ps(
		_mm256_add_ps(
			_mm256_mul_ps(TVecX, PVecX),
			_mm256_add_ps(_mm256_mul_ps(TVecY, PVecY), _mm256_mul_ps(TVecZ, PVecZ))),
		InvDet);

	const __m256 QVecX = _mm256_sub_ps(_mm256_mul_ps(TVecY, Edge1Z), _mm256_mul_ps(TVecZ, Edge1Y));
	const __m256 QVecY = _mm256_sub_ps(_mm256_mul_ps(TVecZ, Edge1X), _mm256_mul_ps(TVecX, Edge1Z));
	const __m256 QVecZ = _mm256_sub_ps(_mm256_mul_ps(TVecX, Edge1Y), _mm256_mul_ps(TVecY, Edge1X));

	const __m256 V = _mm256_mul_ps(
		_mm256_add_ps(
			_mm256_mul_ps(Context.DirectionX, QVecX),
			_mm256_add_ps(_mm256_mul_ps(Context.DirectionY, QVecY), _mm256_mul_ps(Context.DirectionZ, QVecZ))),
		InvDet);
	const __m256 T = _mm256_mul_ps(
		_mm256_add_ps(
			_mm256_mul_ps(Edge2X, QVecX),
			_mm256_add_ps(_mm256_mul_ps(Edge2Y, QVecY), _mm256_mul_ps(Edge2Z, QVecZ))),
		InvDet);

	const __m256 Zero = _mm256_setzero_ps();
	const __m256 One = _mm256_set1_ps(1.0f);
	const __m256 HitMask = _mm256_and_ps(
		_mm256_cmp_ps(AbsDet, _mm256_set1_ps(1e-8f), _CMP_GT_OQ),
		_mm256_and_ps(
			_mm256_cmp_ps(U, Zero, _CMP_GE_OQ),
			_mm256_and_ps(
				_mm256_cmp_ps(U, One, _CMP_LE_OQ),
				_mm256_and_ps(
					_mm256_cmp_ps(V, Zero, _CMP_GE_OQ),
					_mm256_and_ps(
						_mm256_cmp_ps(_mm256_add_ps(U, V), One, _CMP_LE_OQ),
						_mm256_and_ps(
							_mm256_cmp_ps(T, Zero, _CMP_GT_OQ),
							_mm256_cmp_ps(T, _mm256_set1_ps(MaxDistance), _CMP_LT_OQ)))))));

	if (OutTValues)
	{
		_mm256_storeu_ps(OutTValues, T);
	}

	return _mm256_movemask_ps(HitMask);
}

int32 FRayUtilsSIMD::IntersectTriangles8Precomputed(
	const FRaySIMDContext& Context,
	const float* V0X, const float* V0Y, const float* V0Z,
	const float* Edge1X, const float* Edge1Y, const float* Edge1Z,
	const float* Edge2X, const float* Edge2Y, const float* Edge2Z,
	float MaxDistance,
	float* OutTValues)
{
	// SISD 버전이라면 IntersectTriangles8와 거의 같지만,
	// 각 triangle마다 Edge1 = V1 - V0, Edge2 = V2 - V0를
	// raycast 시점에 다시 계산하지 않고 미리 저장된 값을 읽어 쓰는 형태입니다.
	// Leaf packet에서 Edge1/Edge2를 미리 계산해 두었기 때문에 raycast 시에는
	// gather나 재계산 없이 바로 AVX2 연산만 수행한다.
	const __m256 V0XVec = _mm256_load_ps(V0X);
	const __m256 V0YVec = _mm256_load_ps(V0Y);
	const __m256 V0ZVec = _mm256_load_ps(V0Z);
	const __m256 Edge1XVec = _mm256_load_ps(Edge1X);
	const __m256 Edge1YVec = _mm256_load_ps(Edge1Y);
	const __m256 Edge1ZVec = _mm256_load_ps(Edge1Z);
	const __m256 Edge2XVec = _mm256_load_ps(Edge2X);
	const __m256 Edge2YVec = _mm256_load_ps(Edge2Y);
	const __m256 Edge2ZVec = _mm256_load_ps(Edge2Z);

	const __m256 PVecX = _mm256_sub_ps(_mm256_mul_ps(Context.DirectionY, Edge2ZVec), _mm256_mul_ps(Context.DirectionZ, Edge2YVec));
	const __m256 PVecY = _mm256_sub_ps(_mm256_mul_ps(Context.DirectionZ, Edge2XVec), _mm256_mul_ps(Context.DirectionX, Edge2ZVec));
	const __m256 PVecZ = _mm256_sub_ps(_mm256_mul_ps(Context.DirectionX, Edge2YVec), _mm256_mul_ps(Context.DirectionY, Edge2XVec));

	const __m256 Det = _mm256_add_ps(
		_mm256_mul_ps(Edge1XVec, PVecX),
		_mm256_add_ps(_mm256_mul_ps(Edge1YVec, PVecY), _mm256_mul_ps(Edge1ZVec, PVecZ)));
	const __m256 AbsDet = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), Det);
	const __m256 InvDet = _mm256_div_ps(_mm256_set1_ps(1.0f), Det);

	const __m256 TVecX = _mm256_sub_ps(Context.OriginX, V0XVec);
	const __m256 TVecY = _mm256_sub_ps(Context.OriginY, V0YVec);
	const __m256 TVecZ = _mm256_sub_ps(Context.OriginZ, V0ZVec);
	const __m256 U = _mm256_mul_ps(
		_mm256_add_ps(
			_mm256_mul_ps(TVecX, PVecX),
			_mm256_add_ps(_mm256_mul_ps(TVecY, PVecY), _mm256_mul_ps(TVecZ, PVecZ))),
		InvDet);

	const __m256 QVecX = _mm256_sub_ps(_mm256_mul_ps(TVecY, Edge1ZVec), _mm256_mul_ps(TVecZ, Edge1YVec));
	const __m256 QVecY = _mm256_sub_ps(_mm256_mul_ps(TVecZ, Edge1XVec), _mm256_mul_ps(TVecX, Edge1ZVec));
	const __m256 QVecZ = _mm256_sub_ps(_mm256_mul_ps(TVecX, Edge1YVec), _mm256_mul_ps(TVecY, Edge1XVec));

	const __m256 V = _mm256_mul_ps(
		_mm256_add_ps(
			_mm256_mul_ps(Context.DirectionX, QVecX),
			_mm256_add_ps(_mm256_mul_ps(Context.DirectionY, QVecY), _mm256_mul_ps(Context.DirectionZ, QVecZ))),
		InvDet);
	const __m256 T = _mm256_mul_ps(
		_mm256_add_ps(
			_mm256_mul_ps(Edge2XVec, QVecX),
			_mm256_add_ps(_mm256_mul_ps(Edge2YVec, QVecY), _mm256_mul_ps(Edge2ZVec, QVecZ))),
		InvDet);

	const __m256 Zero = _mm256_setzero_ps();
	const __m256 One = _mm256_set1_ps(1.0f);
	const __m256 HitMask = _mm256_and_ps(
		_mm256_cmp_ps(AbsDet, _mm256_set1_ps(1e-8f), _CMP_GT_OQ),
		_mm256_and_ps(
			_mm256_cmp_ps(U, Zero, _CMP_GE_OQ),
			_mm256_and_ps(
				_mm256_cmp_ps(U, One, _CMP_LE_OQ),
				_mm256_and_ps(
					_mm256_cmp_ps(V, Zero, _CMP_GE_OQ),
					_mm256_and_ps(
						_mm256_cmp_ps(_mm256_add_ps(U, V), One, _CMP_LE_OQ),
						_mm256_and_ps(
							_mm256_cmp_ps(T, Zero, _CMP_GT_OQ),
							_mm256_cmp_ps(T, _mm256_set1_ps(MaxDistance), _CMP_LT_OQ)))))));

	if (OutTValues)
	{
		_mm256_storeu_ps(OutTValues, T);
	}

	return _mm256_movemask_ps(HitMask);
}
