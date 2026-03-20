#pragma once

#include "Math/Vector.h"

namespace Geometry
{
    struct FSegment
    {
        FVector Start;
        FVector End;

        constexpr FSegment() : Start(), End() {}

        constexpr FSegment(const FVector &InStart, const FVector &InEnd)
            : Start(InStart), End(InEnd)
        {
        }
    };
} // namespace Geometry