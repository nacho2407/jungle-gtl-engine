#pragma once
#pragma once

#include "Vector.h"

struct FRay;

struct FAABB
{
	FVector Min;
	FVector Max;

	FAABB();
	FAABB(const FVector& InMin, const FVector& InMax);

	void Reset();
	bool IsValid() const;

	void Expand(const FVector& Point);
	void Merge(const FAABB& Other);

	FVector GetCenter() const;
	FVector GetExtent() const;

	bool IntersectRay(const FRay& Ray, float& OutT) const;
};
