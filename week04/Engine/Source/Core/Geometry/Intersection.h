#pragma once

#include <cmath>
#include <limits>
#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Geometry/Primitives/Ray.h"
#include "Core/Geometry/Primitives/Triangle.h"

namespace Geometry
{
    inline bool IntersectRayAABB(const FRay& Ray, const FVector& BoxMin, const FVector& BoxMax,
                                 float&      OutT)
    {
        constexpr float Epsilon = 1e-6f;

        float TMin = 0.0f;
        float TMax = std::numeric_limits<float>::max();

        if (std::fabs(Ray.Direction.X) < Epsilon)
        {
            if (Ray.Origin.X < BoxMin.X || Ray.Origin.X > BoxMax.X)
            {
                return false;
            }
        }
        else
        {
            const float InvD = 1.0f / Ray.Direction.X;
            float       T1 = (BoxMin.X - Ray.Origin.X) * InvD;
            float       T2 = (BoxMax.X - Ray.Origin.X) * InvD;

            if (T1 > T2)
            {
                std::swap(T1, T2);
            }

            TMin = (T1 > TMin) ? T1 : TMin;
            TMax = (T2 < TMax) ? T2 : TMax;

            if (TMax < TMin)
            {
                return false;
            }
        }

        // Y
        if (std::fabs(Ray.Direction.Y) < Epsilon)
        {
            if (Ray.Origin.Y < BoxMin.Y || Ray.Origin.Y > BoxMax.Y)
            {
                return false;
            }
        }
        else
        {
            const float InvD = 1.0f / Ray.Direction.Y;
            float       T1 = (BoxMin.Y - Ray.Origin.Y) * InvD;
            float       T2 = (BoxMax.Y - Ray.Origin.Y) * InvD;

            if (T1 > T2)
            {
                std::swap(T1, T2);
            }

            TMin = (T1 > TMin) ? T1 : TMin;
            TMax = (T2 < TMax) ? T2 : TMax;

            if (TMin > TMax)
            {
                return false;
            }
        }

        // Z
        if (std::fabs(Ray.Direction.Z) < Epsilon)
        {
            if (Ray.Origin.Z < BoxMin.Z || Ray.Origin.Z > BoxMax.Z)
            {
                return false;
            }
        }
        else
        {
            const float InvD = 1.0f / Ray.Direction.Z;
            float       T1 = (BoxMin.Z - Ray.Origin.Z) * InvD;
            float       T2 = (BoxMax.Z - Ray.Origin.Z) * InvD;

            if (T1 > T2)
            {
                std::swap(T1, T2);
            }

            TMin = (T1 > TMin) ? T1 : TMin;
            TMax = (T2 < TMax) ? T2 : TMax;

            if (TMin > TMax)
            {
                return false;
            }
        }

        OutT = TMin;
        return true;
    }

    inline bool IntersectRayAABB(const FRay& Ray, const FAABB& Box, float& OutT)
    {
        return IntersectRayAABB(Ray, Box.Min, Box.Max, OutT);
    }

    inline bool IntersectRayTriangle(const FRay&    Ray, const FVector& V0, const FVector& V1,
                                     const FVector& V2, float&          OutT)
    {
	    constexpr float Epsilon = std::numeric_limits<float>::epsilon();

        const FVector Edge1 = V1 - V0;
        const FVector Edge2 = V2 - V0;

        const FVector PVec = FVector::CrossProduct(Ray.Direction, Edge2);
        const float   Det = FVector::DotProduct(Edge1, PVec);

        if (std::fabs(Det) < Epsilon)
        {
            return false;
        }

        const float InvDet = 1.0f / Det;

        //  U
        const FVector TVec = Ray.Origin - V0;
        const float   U = FVector::DotProduct(TVec, PVec) * InvDet;
        if (U < 0.0f || U > 1.0f)
        {
            return false;
        }

        //  V
        const FVector QVec = FVector::CrossProduct(TVec, Edge1);
        const float   V = FVector::DotProduct(Ray.Direction, QVec) * InvDet;
        if (V < 0.0f || (U + V) > 1.0f)
        {
            return false;
        }

        //  T
        const float T = FVector::DotProduct(Edge2, QVec) * InvDet;
        if (T < 0.0f)
        {
            return false;
        }
        OutT = T;
        return true;
    }

    inline bool IntersectRayTriangle(const FRay& Ray, const FTriangle& Triangle, float& OutT)
    {
        return IntersectRayTriangle(Ray, Triangle.V0, Triangle.V1, Triangle.V2, OutT);
    }
} // namespace Geometry