// 수학 영역에서 공유되는 교차 판정 유틸리티입니다.
#pragma once

#include <algorithm>
#include <cmath>

#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Core/RayTypes.h"
#include "Math/Rotator.h"

namespace FMath
{
inline bool IntersectRayAABB(const FRay& Ray, const FVector& AABBMin, const FVector& AABBMax, float& OutTMin, float& OutTMax)
{
    float tMin = -INFINITY;
    float tMax = INFINITY;

    const float* Origin = &Ray.Origin.X;
    const float* Direction = &Ray.Direction.X;
    const float* MinBounds = &AABBMin.X;
    const float* MaxBounds = &AABBMax.X;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (std::abs(Direction[Axis]) < 1e-8f)
        {
            if (Origin[Axis] < MinBounds[Axis] || Origin[Axis] > MaxBounds[Axis])
            {
                return false;
            }
            continue;
        }

        float InvDir = 1.0f / Direction[Axis];
        float T1 = (MinBounds[Axis] - Origin[Axis]) * InvDir;
        float T2 = (MaxBounds[Axis] - Origin[Axis]) * InvDir;

        if (T1 > T2)
        {
            std::swap(T1, T2);
        }

        tMin = (std::max)(tMin, T1);
        tMax = (std::min)(tMax, T2);
        if (tMin > tMax)
        {
            return false;
        }
    }

    OutTMin = tMin;
    OutTMax = tMax;
    return tMax >= 0.0f;
}

inline bool IntersectRayAABB(const FRay& Ray, const FBoundingBox& Bounds, float& OutTMin, float& OutTMax)
{
    return IntersectRayAABB(Ray, Bounds.Min, Bounds.Max, OutTMin, OutTMax);
}

inline bool CheckRayAABB(const FRay& Ray, const FVector& AABBMin, const FVector& AABBMax)
{
    float TMin = 0.0f;
    float TMax = 0.0f;
    return IntersectRayAABB(Ray, AABBMin, AABBMax, TMin, TMax);
}

inline bool CheckRayAABB(const FRay& Ray, const FBoundingBox& Bounds)
{
    return CheckRayAABB(Ray, Bounds.Min, Bounds.Max);
}

inline bool IntersectAABB(const FBoundingBox& A, const FBoundingBox& B)
{
    return A.Min.X <= B.Max.X && A.Max.X >= B.Min.X &&
           A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y &&
           A.Min.Z <= B.Max.Z && A.Max.Z >= B.Min.Z;
}

inline bool IntersectTriangle(
    const FVector& RayOrigin,
    const FVector& RayDirection,
    const FVector& V0,
    const FVector& V1,
    const FVector& V2,
    float& OutT)
{
    const FVector Edge1 = V1 - V0;
    const FVector Edge2 = V2 - V0;
    const FVector PVec = RayDirection.Cross(Edge2);
    const float Determinant = Edge1.Dot(PVec);

    if (std::abs(Determinant) < 0.0001f)
    {
        return false;
    }

    const float InvDeterminant = 1.0f / Determinant;
    const FVector TVec = RayOrigin - V0;
    const float U = TVec.Dot(PVec) * InvDeterminant;
    if (U < 0.0f || U > 1.0f)
    {
        return false;
    }

    const FVector QVec = TVec.Cross(Edge1);
    const float V = RayDirection.Dot(QVec) * InvDeterminant;
    if (V < 0.0f || U + V > 1.0f)
    {
        return false;
    }

    OutT = Edge2.Dot(QVec) * InvDeterminant;
    return OutT > 0.0f;
}

inline bool IntersectSphereAABB(const FVector& Center, float Radius, const FBoundingBox& Box)
{
    float DistanceSq = 0.0f;
    const float RadiusSq = Radius * Radius;

    if (Center.X < Box.Min.X) DistanceSq += (Center.X - Box.Min.X) * (Center.X - Box.Min.X);
    else if (Center.X > Box.Max.X) DistanceSq += (Center.X - Box.Max.X) * (Center.X - Box.Max.X);

    if (Center.Y < Box.Min.Y) DistanceSq += (Center.Y - Box.Min.Y) * (Center.Y - Box.Min.Y);
    else if (Center.Y > Box.Max.Y) DistanceSq += (Center.Y - Box.Max.Y) * (Center.Y - Box.Max.Y);

    if (Center.Z < Box.Min.Z) DistanceSq += (Center.Z - Box.Min.Z) * (Center.Z - Box.Min.Z);
    else if (Center.Z > Box.Max.Z) DistanceSq += (Center.Z - Box.Max.Z) * (Center.Z - Box.Max.Z);

    return DistanceSq <= RadiusSq;
}

inline bool IntersectOBBAABB(const FVector& Center, const FVector& Extent, const FRotator& Rotation, const FBoundingBox& AABB)
{
    const FVector AABBCenter = AABB.GetCenter();
    const FVector AABBExtent = AABB.GetExtent();
    const FVector OBBAxes[3] = {
        Rotation.GetForwardVector().Normalized(),
        Rotation.GetRightVector().Normalized(),
        Rotation.GetUpVector().Normalized()
    };
    const FVector AABBAxes[3] = {
        FVector(1.0f, 0.0f, 0.0f),
        FVector(0.0f, 1.0f, 0.0f),
        FVector(0.0f, 0.0f, 1.0f)
    };

    float RotationMatrix[3][3];
    float AbsRotation[3][3];
    for (int32 I = 0; I < 3; ++I)
    {
        for (int32 J = 0; J < 3; ++J)
        {
            RotationMatrix[I][J] = OBBAxes[I].Dot(AABBAxes[J]);
            AbsRotation[I][J] = std::abs(RotationMatrix[I][J]) + 1e-6f;
        }
    }

    const FVector Translation = AABBCenter - Center;
    const FVector LocalTranslation(
        Translation.Dot(OBBAxes[0]),
        Translation.Dot(OBBAxes[1]),
        Translation.Dot(OBBAxes[2]));

    float RadiusA = 0.0f;
    float RadiusB = 0.0f;

    for (int32 I = 0; I < 3; ++I)
    {
        RadiusA = Extent.Data[I];
        RadiusB = AABBExtent.X * AbsRotation[I][0] + AABBExtent.Y * AbsRotation[I][1] + AABBExtent.Z * AbsRotation[I][2];
        if (std::abs(LocalTranslation.Data[I]) > RadiusA + RadiusB)
        {
            return false;
        }
    }

    for (int32 J = 0; J < 3; ++J)
    {
        RadiusA = Extent.X * AbsRotation[0][J] + Extent.Y * AbsRotation[1][J] + Extent.Z * AbsRotation[2][J];
        RadiusB = AABBExtent.Data[J];
        const float Distance = std::abs(
            LocalTranslation.X * RotationMatrix[0][J] +
            LocalTranslation.Y * RotationMatrix[1][J] +
            LocalTranslation.Z * RotationMatrix[2][J]);
        if (Distance > RadiusA + RadiusB)
        {
            return false;
        }
    }

    for (int32 I = 0; I < 3; ++I)
    {
        for (int32 J = 0; J < 3; ++J)
        {
            RadiusA = Extent.Data[(I + 1) % 3] * AbsRotation[(I + 2) % 3][J] + Extent.Data[(I + 2) % 3] * AbsRotation[(I + 1) % 3][J];
            RadiusB = AABBExtent.Data[(J + 1) % 3] * AbsRotation[I][(J + 2) % 3] + AABBExtent.Data[(J + 2) % 3] * AbsRotation[I][(J + 1) % 3];
            const float Distance = std::abs(
                LocalTranslation.Data[(I + 2) % 3] * RotationMatrix[(I + 1) % 3][J] -
                LocalTranslation.Data[(I + 1) % 3] * RotationMatrix[(I + 2) % 3][J]);
            if (Distance > RadiusA + RadiusB)
            {
                return false;
            }
        }
    }

    return true;
}
} // namespace FMath
