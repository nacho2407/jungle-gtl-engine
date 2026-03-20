#pragma once

#include "Core/Math/Vector.h"

namespace Geometry
{
    struct FAABB
    {
        FVector Min;
        FVector Max;

        constexpr FAABB() : Min(), Max() {}

        constexpr FAABB(const FVector &InMin, const FVector &InMax) : Min(InMin), Max(InMax) {}
    };
} // namespace Geometry