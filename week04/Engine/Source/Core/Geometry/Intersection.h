#pragma once

#include <cmath>
#include <limits>
#include "Geometry/Primitives/AABB.h"
#include "Geometry/Primitives/Ray.h"
#include "Geometry/Primitives/Triangle.h"

namespace Geometry
{
    inline bool IntersectRayAABB(const FRay &Ray, const FVector &BoxMin, const FVector &BoxMax,
                                 float &OutT)
    {
        // ...
    }

    inline bool IntersectRayAABB(const FRay &Ray, const FAABB &Box, float &OutT)
    {
        return IntersectRayAABB(Ray, Box.Min, Box.Max, OutT);
    }

    inline bool IntersectRayTriangle(const FRay &Ray, const FVector &V0, const FVector &V1,
                                     const FVector &V2, float &OutT)
    {
        // ...
    }

    inline bool IntersectRayTriangle(const FRay &Ray, const FTriangle &Triangle, float &OutT)
    {
        return IntersectRayTriangle(Ray, Triangle.V0, Triangle.V1, Triangle.V2, OutT);
    }
} // namespace Geometry
