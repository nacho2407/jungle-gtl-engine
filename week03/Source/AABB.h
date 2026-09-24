#pragma once

#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Ray.h"
#include <algorithm>

struct FAABB
{
	FVector MinPos = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxPos = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	FMatrix RelativeMatrix = FMatrix::IdentityMatrix();

	// Slab Method를 이용한 AABB 레이 교차 검사 (성능 최적화용)
	// 각 x, y, z에서 min, max를 구해서 레이가 통과하는 순서를 통해 빠르게 계산합니다.
	inline bool RayIntersects(const Ray& ray, float& OutT) const
	{
		FMatrix InvWorld = RelativeMatrix.Inverse();
		FVector LocalOrigin = InvWorld.TransformPoint(ray.Origin);
		FVector LocalDir = InvWorld.TransformVector(ray.Direction);

		float tmin = -FLT_MAX, tmax = FLT_MAX;
		const float* d_ptr = &LocalDir.x; const float* o_ptr = &LocalOrigin.x;
		const float* min_ptr = &MinPos.x; const float* max_ptr = &MaxPos.x;
		for (int i = 0; i < 3; ++i)
		{
			if (std::abs(d_ptr[i]) < 1e-6f) {
				if (o_ptr[i] < min_ptr[i] || o_ptr[i] > max_ptr[i]) {
					return false;
				}
			}
			else
			{
				float t1 = (min_ptr[i] - o_ptr[i]) / d_ptr[i], t2 = (max_ptr[i] - o_ptr[i]) / d_ptr[i];
				if (t1 > t2)
				{
					std::swap(t1, t2);
				}
				tmin = (std::max)(tmin, t1); tmax = (std::min)(tmax, t2);
				if (tmin > tmax)
				{
					return false;
				}
			}
		}
		if (tmax < 0)
		{
			return false;
		}
		OutT = tmin < 0 ? tmax : tmin;
		return true;
	}
};
