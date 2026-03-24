#pragma once

#include "Core/Math/Vector.h"
#include <algorithm>

namespace Geometry
{
    inline void ExpandAABB(const FVector &InPoint, FVector &InOutMin, FVector &InOutMax)
    {
        InOutMin.X = std::min(InOutMin.X, InPoint.X);
        InOutMin.Y = std::min(InOutMin.Y, InPoint.Y);
        InOutMin.Z = std::min(InOutMin.Z, InPoint.Z);

        InOutMax.X = std::max(InOutMax.X, InPoint.X);
        InOutMax.Y = std::max(InOutMax.Y, InPoint.Y);
        InOutMax.Z = std::max(InOutMax.Z, InPoint.Z);
    }
} // namespace Geometry