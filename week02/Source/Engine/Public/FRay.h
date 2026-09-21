#pragma once
#include "Math/FVector.h"

struct FRay 
{
	FVector Origin; // 광선의 시작점
	FVector Direction; // 광선의 방향 벡터 (정규화된)

    FRay() = default;

    FRay(const FVector& InOrigin, const FVector& InDirection)
        : Origin(InOrigin)
        , Direction(InDirection.GetNormalized())
    {
    }

    FVector PointAt(float T) const
    {
        return Origin + Direction * T;
    }
};