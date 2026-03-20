#pragma once

#include "Math/Vector.h"

namespace Geometry
{
    struct FRay
    {
        FVector Origin;
        FVector Direction;

        constexpr FRay() : Origin(), Direction() {}

        constexpr FRay(const FVector &InOrigin, const FVector &InDirection)
            : Origin(InOrigin), Direction(InDirection)
        {
        }
    };
} // namespace Geometry