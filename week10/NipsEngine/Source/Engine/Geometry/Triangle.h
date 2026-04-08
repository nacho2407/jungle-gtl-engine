#pragma once

#include "Engine/Math/Vector.h"

struct FTriangle
{
	FVector V0;
	FVector V1;
	FVector V2;

	constexpr FTriangle() : V0(), V1(), V2() {}

	constexpr FTriangle(const FVector& InV0, const FVector& InV1, const FVector& InV2)
		: V0(InV0), V1(InV1), V2(InV2)
	{
	}
};
