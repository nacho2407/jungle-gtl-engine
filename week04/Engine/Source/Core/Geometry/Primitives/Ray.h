#pragma once

#include "Core/Math/Vector.h"
#include "Core/Math/Matrix.h"

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

        static FRay BuildRay(int32 MouseX, int32 MouseY, const FMatrix& ViewProjection, float ViewportWidth,
                      float ViewportHeight)
        {
            if (ViewportWidth <= 0 || ViewportHeight <= 0)
            {
                return Geometry::FRay{};
            }

            const float NDCX =
                (2.0f * static_cast<float>(MouseX) / static_cast<float>(ViewportWidth) - 1.0f);
            const float NDCY =
                1.0f - (2.0f * static_cast<float>(MouseY) / static_cast<float>(ViewportHeight));

            const FVector NearPointNDC(NDCX, NDCY, 0.0f);
            const FVector FarPointNDC(NDCX, NDCY, 1.0f);

            const FMatrix InvViewProjection = ViewProjection.GetInverse();

            const FVector NearWorld = InvViewProjection.TransformPosition(NearPointNDC);
            const FVector FarWorld = InvViewProjection.TransformPosition(FarPointNDC);

            const FVector Direction = (FarWorld - NearWorld).GetSafeNormal();

            return Geometry::FRay{NearWorld, Direction};
        }
    };
} // namespace Geometry