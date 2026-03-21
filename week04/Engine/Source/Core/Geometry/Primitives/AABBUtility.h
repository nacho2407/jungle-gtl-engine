#pragma once

#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector.h"
#include <cfloat>

namespace Geometry
{
    inline FAABB TransformAABB(const FAABB& InLocalAABB, const FMatrix& InMatrix)
    {
        const FVector& Min = InLocalAABB.Min;
        const FVector& Max = InLocalAABB.Max;

        const FVector Corners[8] = {
            FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z),
            FVector(Min.X, Max.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z),
            FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z),
            FVector(Min.X, Max.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z),
        };

        FVector NewMin(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector NewMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (const FVector& Corner : Corners)
        {
            const FVector P = InMatrix.TransformPosition(Corner);

            NewMin.X = (P.X < NewMin.X) ? P.X : NewMin.X;
            NewMin.Y = (P.Y < NewMin.Y) ? P.Y : NewMin.Y;
            NewMin.Z = (P.Z < NewMin.Z) ? P.Z : NewMin.Z;

            NewMax.X = (P.X > NewMax.X) ? P.X : NewMax.X;
            NewMax.Y = (P.Y > NewMax.Y) ? P.Y : NewMax.Y;
            NewMax.Z = (P.Z > NewMax.Z) ? P.Z : NewMax.Z;
        }

        return FAABB(NewMin, NewMax);
    }
} // namespace Geometry