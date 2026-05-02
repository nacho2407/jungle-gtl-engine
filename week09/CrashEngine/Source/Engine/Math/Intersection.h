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

	
// 선분과 점 사이의 최단거리 점 임시 (컴파일을 위해 대상 Point를 그대로 반환)
inline FVector ClosestPointOnSegment(
    const FVector& Point, const FVector& LineStart, const FVector& LineEnd)
{
    FVector SegmentDir = LineEnd - LineStart;
    float SegmentLengthSq = SegmentDir.LengthSquared();

    // 선분의 길이가 0인 경우 (점인 경우)
    if (SegmentLengthSq < 1e-6f)
    {
        return LineStart;
    }

    // 선분 방향으로 Point를 투영 (t는 0.0 ~ 1.0 사이로 제한)
    float t = (Point - LineStart).Dot(SegmentDir) / SegmentLengthSq;
    t = std::clamp(t, 0.0f, 1.0f);

    return LineStart + SegmentDir * t;
}

// 선분과 선분 사이의 최단거리 점 두 개 임시 (컴파일을 위해 각 선분의 시작점을 반환)
inline void ClosestPointsBetweenSegments(
    const FVector& Line1Start, const FVector& Line1End,
    const FVector& Line2Start, const FVector& Line2End,
    FVector& OutPoint1, FVector& OutPoint2)
{
    FVector d1 = Line1End - Line1Start;
    FVector d2 = Line2End - Line2Start;
    FVector r = Line1Start - Line2Start;

    float a = d1.LengthSquared();
    float e = d2.LengthSquared();
    float f = d2.Dot(r);

    float s = 0.0f;
    float t = 0.0f;

    if (a <= 1e-6f && e <= 1e-6f)
    {
        OutPoint1 = Line1Start;
        OutPoint2 = Line2Start;
        return;
    }

    if (a <= 1e-6f)
    {
        s = 0.0f;
        t = std::clamp(f / e, 0.0f, 1.0f);
    }
    else
    {
        float c = d1.Dot(r);
        if (e <= 1e-6f)
        {
            t = 0.0f;
            s = std::clamp(-c / a, 0.0f, 1.0f);
        }
        else
        {
            float b = d1.Dot(d2);
            float denom = a * e - b * b;

            if (denom != 0.0f)
            {
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            }
            else
            {
                s = 0.0f;
            }

            t = (b * s + f) / e;
            if (t < 0.0f)
            {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }

    OutPoint1 = Line1Start + d1 * s;
    OutPoint2 = Line2Start + d2 * t;
}
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
// OBB vs OBB 임시
inline bool IntersectOBBOBB(
    const FVector& CenterA, const FVector& ExtentA, const FRotator& RotA,
    const FVector& CenterB, const FVector& ExtentB, const FRotator& RotB)
{
    FVector AxisA[3];
    AxisA[0] = RotA.GetForwardVector();
    AxisA[1] = RotA.GetRightVector();
    AxisA[2] = RotA.GetUpVector();

    FVector AxisB[3];
    AxisB[0] = RotB.GetForwardVector();
    AxisB[1] = RotB.GetRightVector();
    AxisB[2] = RotB.GetUpVector();

    float ExtA[3] = { ExtentA.X, ExtentA.Y, ExtentA.Z };
    float ExtB[3] = { ExtentB.X, ExtentB.Y, ExtentB.Z };

    // -------------------------------------------------------------------------
    // 2. 거리 벡터 및 내적 캐싱
    // -------------------------------------------------------------------------
    // 두 중심점 간의 벡터를 구하고, 이를 A의 로컬 공간으로 투영합니다.
    FVector V = CenterB - CenterA;
    float T[3] = { V.Dot(AxisA[0]), V.Dot(AxisA[1]), V.Dot(AxisA[2]) };

    float R[3][3];
    float AbsR[3][3];

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            R[i][j] = AxisA[i].Dot(AxisB[j]);
            AbsR[i][j] = std::abs(R[i][j]) + EPSILON;
        }
    }

    float ra, rb;

    // -------------------------------------------------------------------------
    // 3. 분리축 정리 (SAT) - 15개 축 테스트
    // -------------------------------------------------------------------------

    // Test 1~3: A의 로컬 축 3개
    for (int i = 0; i < 3; i++)
    {
        ra = ExtA[i];
        rb = ExtB[0] * AbsR[i][0] + ExtB[1] * AbsR[i][1] + ExtB[2] * AbsR[i][2];
        if (std::abs(T[i]) > ra + rb)
            return false;
    }

    // Test 4~6: B의 로컬 축 3개
    for (int i = 0; i < 3; i++)
    {
        ra = ExtA[0] * AbsR[0][i] + ExtA[1] * AbsR[1][i] + ExtA[2] * AbsR[2][i];
        rb = ExtB[i];
        if (std::abs(V.Dot(AxisB[i])) > ra + rb)
            return false;
    }

    // Test 7~15: A의 축 3개 x B의 축 3개를 외적(Cross Product)하여 나오는 9개의 직교 축
    // (연산 최적화를 위해 외적 공식을 스칼라 연산으로 미리 풀어서 하드코딩된 상태입니다)

    // 7. A0 x B0
    ra = ExtA[1] * AbsR[2][0] + ExtA[2] * AbsR[1][0];
    rb = ExtB[1] * AbsR[0][2] + ExtB[2] * AbsR[0][1];
    if (std::abs(T[2] * R[1][0] - T[1] * R[2][0]) > ra + rb)
        return false;

    // 8. A0 x B1
    ra = ExtA[1] * AbsR[2][1] + ExtA[2] * AbsR[1][1];
    rb = ExtB[0] * AbsR[0][2] + ExtB[2] * AbsR[0][0];
    if (std::abs(T[2] * R[1][1] - T[1] * R[2][1]) > ra + rb)
        return false;

    // 9. A0 x B2
    ra = ExtA[1] * AbsR[2][2] + ExtA[2] * AbsR[1][2];
    rb = ExtB[0] * AbsR[0][1] + ExtB[1] * AbsR[0][0];
    if (std::abs(T[2] * R[1][2] - T[1] * R[2][2]) > ra + rb)
        return false;

    // 10. A1 x B0
    ra = ExtA[0] * AbsR[2][0] + ExtA[2] * AbsR[0][0];
    rb = ExtB[1] * AbsR[1][2] + ExtB[2] * AbsR[1][1];
    if (std::abs(T[0] * R[2][0] - T[2] * R[0][0]) > ra + rb)
        return false;

    // 11. A1 x B1
    ra = ExtA[0] * AbsR[2][1] + ExtA[2] * AbsR[0][1];
    rb = ExtB[0] * AbsR[1][2] + ExtB[2] * AbsR[1][0];
    if (std::abs(T[0] * R[2][1] - T[2] * R[0][1]) > ra + rb)
        return false;

    // 12. A1 x B2
    ra = ExtA[0] * AbsR[2][2] + ExtA[2] * AbsR[0][2];
    rb = ExtB[0] * AbsR[1][1] + ExtB[1] * AbsR[1][0];
    if (std::abs(T[0] * R[2][2] - T[2] * R[0][2]) > ra + rb)
        return false;

    // 13. A2 x B0
    ra = ExtA[0] * AbsR[1][0] + ExtA[1] * AbsR[0][0];
    rb = ExtB[1] * AbsR[2][2] + ExtB[2] * AbsR[2][1];
    if (std::abs(T[1] * R[0][0] - T[0] * R[1][0]) > ra + rb)
        return false;

    // 14. A2 x B1
    ra = ExtA[0] * AbsR[1][1] + ExtA[1] * AbsR[0][1];
    rb = ExtB[0] * AbsR[2][2] + ExtB[2] * AbsR[2][0];
    if (std::abs(T[1] * R[0][1] - T[0] * R[1][1]) > ra + rb)
        return false;

    // 15. A2 x B2
    ra = ExtA[0] * AbsR[1][2] + ExtA[1] * AbsR[0][2];
    rb = ExtB[0] * AbsR[2][1] + ExtB[1] * AbsR[2][0];
    if (std::abs(T[1] * R[0][2] - T[0] * R[1][2]) > ra + rb)
        return false;

    // 위 15개의 분리축 테스트를 모두 통과했다면, 그림자가 겹치지 않는 공간이 없다는 뜻!
    // 즉, 두 OBB는 교차(충돌)하고 있습니다.
    return true;
}

// Sphere vs OBB 임시
inline bool IntersectSphereOBB(
    const FVector& SphereCenter, float SphereRadius,
    const FVector& BoxCenter, const FVector& BoxExtent, const FRotator& BoxRot)
{
    // 1. 구의 중심을 BoxCenter 기준으로 평행이동
    FVector LocalCenter = SphereCenter - BoxCenter;

    // 2. OBB의 3개 축
    FVector BoxAxes[3] = {
        BoxRot.GetForwardVector(),
        BoxRot.GetRightVector(),
        BoxRot.GetUpVector()
    };

    // 3. 로컬 공간에서의 중심 좌표 계산 (각 축에 투영)
    float DistX = LocalCenter.Dot(BoxAxes[0]);
    float DistY = LocalCenter.Dot(BoxAxes[1]);
    float DistZ = LocalCenter.Dot(BoxAxes[2]);

    // 4. BoxExtent 범위로 클램핑하여 가장 가까운 점(Closest Point) 찾기
    float ClosestX = std::clamp(DistX, -BoxExtent.X, BoxExtent.X);
    float ClosestY = std::clamp(DistY, -BoxExtent.Y, BoxExtent.Y);
    float ClosestZ = std::clamp(DistZ, -BoxExtent.Z, BoxExtent.Z);

    // 5. 가장 가까운 점의 월드 좌표 복원
    FVector ClosestPoint = BoxCenter + BoxAxes[0] * ClosestX + BoxAxes[1] * ClosestY + BoxAxes[2] * ClosestZ;

    // 6. 구의 중심과 가장 가까운 점 사이의 거리가 반지름보다 작으면 충돌
    float DistanceSq = (ClosestPoint - SphereCenter).LengthSquared();
    return DistanceSq <= (SphereRadius * SphereRadius);
}

// OBB vs Capsule 임시
inline bool IntersectOBBCapsule(
    const FVector& BoxCenter, const FVector& BoxExtent, const FRotator& BoxRot,
    const FVector& CapCenter, float CapHalfHeight, float CapRadius, const FVector& CapUpVector)
{
    FVector CapDir = CapUpVector.Normalized();

    // 캡슐 내부의 중심 선분 양 끝점
    FVector CapSegmentStart = CapCenter - CapDir * CapHalfHeight;
    FVector CapSegmentEnd = CapCenter + CapDir * CapHalfHeight;

    // BoxCenter에서 캡슐 선분 중 가장 가까운 지점을 찾음
    FVector ClosestOnCapsuleSegment = ClosestPointOnSegment(BoxCenter, CapSegmentStart, CapSegmentEnd);

    // 해당 지점을 중심으로 하고 캡슐의 반지름을 가지는 가상의 구(Sphere)와 OBB를 충돌 검사
    return IntersectSphereOBB(ClosestOnCapsuleSegment, CapRadius, BoxCenter, BoxExtent, BoxRot);

}


} // namespace FMath
