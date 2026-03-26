#pragma once

#include "Math/Vector.h"

// 전방 선언을 통해 순환 참조 해결
class UPrimitiveComponent;

struct Ray
{
	FVector Origin;
	FVector Direction;

	Ray() = default;
	Ray(FVector from, FVector dir);

	bool IntersectsTriangle(const FVector& V0, const FVector& V1, const FVector& V2, float& OutT, const Ray& ray);

	bool Intersects(UPrimitiveComponent* Comp, float& OutDistance);
};
