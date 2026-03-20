#pragma once

#include "Math/Vector.h"
#include <algorithm>

namespace Geometry
{
    inline void ExpandAABB(const FVector &InPoint, FVector &InOutMin, FVector &InOutMax)
    {
        InOutMin.x = std::min(InOutMin.x, InPoint.x);
        InOutMin.y = std::min(InOutMin.y, InPoint.y);
        InOutMin.z = std::min(InOutMin.z, InPoint.z);

        InOutMax.x = std::max(InOutMax.x, InPoint.x);
        InOutMax.y = std::max(InOutMax.y, InPoint.y);
        InOutMax.z = std::max(InOutMax.z, InPoint.z);
    }
} // namespace Geometry