#pragma once

#include "Math/Vector.h"
#include "Component/PrimitiveComponent.h"

class CameraComponent;

struct Ray
{
	FVector Origin;
	FVector Direction;

	Ray() = default;
	Ray(FVector from, FVector dir);

	bool IntersectsTriangle(const FVector& V0, const FVector& V1, const FVector& V2, float& OutT, const Ray& ray);

	bool Intersects(UPrimitiveComponent* Comp, float& OutDistance);
};
