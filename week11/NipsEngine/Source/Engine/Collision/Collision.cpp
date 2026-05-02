#include "Collision.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "Core/CoreMinimal.h"
#include <algorithm>

#define KINDA_SMALL_NUMBER 1e-6

FCollisionResult FCollision::CheckOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B)
{
    FCollisionResult Empty;

    if (!A || !B)
        return Empty;

    if (!A->ShouldGenerateOverlapEvents() || !B->ShouldGenerateOverlapEvents())
        return Empty;

    auto TypeA = A->GetPrimitiveType();
    auto TypeB = B->GetPrimitiveType();

    if (TypeA == EPrimitiveType::EPT_Box && TypeB == EPrimitiveType::EPT_Box)
        return IntersectBoxBox((UBoxComponent*)A, (UBoxComponent*)B);

    if (TypeA == EPrimitiveType::EPT_Sphere && TypeB == EPrimitiveType::EPT_Sphere)
        return IntersectSphereSphere((USphereComponent*)A, (USphereComponent*)B);

    if (TypeA == EPrimitiveType::EPT_Box && TypeB == EPrimitiveType::EPT_Sphere)
        return IntersectBoxSphere((UBoxComponent*)A, (USphereComponent*)B);

    if (TypeA == EPrimitiveType::EPT_Sphere && TypeB == EPrimitiveType::EPT_Box)
        return IntersectBoxSphere((UBoxComponent*)B, (USphereComponent*)A);

    if (TypeA == EPrimitiveType::EPT_Capsule && TypeB == EPrimitiveType::EPT_Capsule)
        return IntersectCapsuleCapsule((UCapsuleComponent*)A, (UCapsuleComponent*)B);

    if (TypeA == EPrimitiveType::EPT_Capsule && TypeB == EPrimitiveType::EPT_Sphere)
        return IntersectCapsuleSphere((UCapsuleComponent*)A, (USphereComponent*)B);

    if (TypeA == EPrimitiveType::EPT_Sphere && TypeB == EPrimitiveType::EPT_Capsule)
        return IntersectCapsuleSphere((UCapsuleComponent*)B, (USphereComponent*)A);

    if (TypeA == EPrimitiveType::EPT_Capsule && TypeB == EPrimitiveType::EPT_Box)
        return IntersectCapsuleBox((UCapsuleComponent*)A, (UBoxComponent*)B);

    if (TypeA == EPrimitiveType::EPT_Box && TypeB == EPrimitiveType::EPT_Capsule)
        return IntersectCapsuleBox((UCapsuleComponent*)B, (UBoxComponent*)A);

    return Empty;
}

float FCollision::SegmentSegmentDistSq(
    const FVector& P1, const FVector& Q1,
    const FVector& P2, const FVector& Q2)
{
    FVector d1 = Q1 - P1;
    FVector d2 = Q2 - P2;
    FVector r = P1 - P2;

    float a = d1.DotProduct(d1);
    float e = d2.DotProduct(d2);
    float f = d2.DotProduct(r);

    float s, t;

    if (a <= KINDA_SMALL_NUMBER && e <= KINDA_SMALL_NUMBER)
        return (P1 - P2).SizeSquared();

    if (a <= KINDA_SMALL_NUMBER)
    {
        s = 0.0f;
        t = std::clamp(f / e, 0.0f, 1.0f);
    }
    else
    {
        float c = d1.DotProduct(r);
        if (e <= KINDA_SMALL_NUMBER)
        {
            t = 0.0f;
            s = std::clamp(-c / a, 0.0f, 1.0f);
        }
        else
        {
            float b = d1.DotProduct(d2);
            float denom = a * e - b * b;

            if (denom != 0.0f)
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            else
                s = 0.0f;

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

    FVector C1 = P1 + d1 * s;
    FVector C2 = P2 + d2 * t;

    return (C1 - C2).SizeSquared();
}

FCollisionResult FCollision::IntersectBoxBox(const UBoxComponent* A, const UBoxComponent* B)
{
    const FOBB OBB_A = A->GetWorldOBB();
    const FOBB OBB_B = B->GetWorldOBB();

    return IntersectOBB(OBB_A, OBB_B);
}

FCollisionResult FCollision::IntersectOBB(const FOBB& A, const FOBB& B)
{
    FCollisionResult Result;

    FVector AAxis[3], BAxis[3];
    A.GetAxes(AAxis[0], AAxis[1], AAxis[2]);
    B.GetAxes(BAxis[0], BAxis[1], BAxis[2]);

    const FVector& EA = A.Extents;
    const FVector& EB = B.Extents;

    FVector T = B.Center - A.Center;

    float R[3][3], AbsR[3][3];

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
        {
            R[i][j] = AAxis[i].DotProduct(BAxis[j]);
            AbsR[i][j] = MathUtil::Abs(R[i][j]) + KINDA_SMALL_NUMBER;
        }

    for (int i = 0; i < 3; i++)
    {
        float ra = EA[i];
        float rb =
            EB.X * AbsR[i][0] +
            EB.Y * AbsR[i][1] +
            EB.Z * AbsR[i][2];

        if (MathUtil::Abs(T.DotProduct(AAxis[i])) > ra + rb)
            return Result;
    }

    for (int i = 0; i < 3; i++)
    {
        float ra =
            EA.X * AbsR[0][i] +
            EA.Y * AbsR[1][i] +
            EA.Z * AbsR[2][i];

        float rb = EB[i];

        if (MathUtil::Abs(T.DotProduct(BAxis[i])) > ra + rb)
            return Result;
    }

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
        {
            FVector axis = AAxis[i].CrossProduct(BAxis[j]);

            if (axis.SizeSquared() < KINDA_SMALL_NUMBER)
                continue;

            axis.Normalize();

            float ra =
                EA[(i + 1) % 3] * MathUtil::Abs(AAxis[(i + 1) % 3].DotProduct(axis)) +
                EA[(i + 2) % 3] * MathUtil::Abs(AAxis[(i + 2) % 3].DotProduct(axis));

            float rb =
                EB[(j + 1) % 3] * MathUtil::Abs(BAxis[(j + 1) % 3].DotProduct(axis)) +
                EB[(j + 2) % 3] * MathUtil::Abs(BAxis[(j + 2) % 3].DotProduct(axis));

            float t =
                MathUtil::Abs((B.Center - A.Center).DotProduct(axis));

            if (t > ra + rb)
                return Result;
        }

    Result.bHit = true;
    Result.HitPoint = (A.Center + B.Center) * 0.5f;
    return Result;
}

FVector FCollision::ClosestPointOnOBB(const FVector& P, const FOBB& Box)
{
    FVector d = P - Box.Center;

    FVector X, Y, Z;
    Box.GetAxes(X, Y, Z);

    FVector result = Box.Center;

    FVector axis[3] = { X, Y, Z };

    for (int i = 0; i < 3; i++)
    {
        float dist = d.DotProduct(axis[i]);
        dist = std::clamp(dist, -Box.Extents[i], Box.Extents[i]);
        result += axis[i] * dist;
    }

    return result;
}

FCollisionResult FCollision::IntersectSphereSphere(const USphereComponent* A, const USphereComponent* B)
{
    FCollisionResult Result;

    FVector CenterA = A->GetWorldLocation();
    FVector CenterB = B->GetWorldLocation();

    float RadiusA = A->GetScaledSphereRadius();
    float RadiusB = B->GetScaledSphereRadius();

    float DistSq = (CenterA - CenterB).SizeSquared();
    float RadiusSum = RadiusA + RadiusB;

    if (DistSq <= RadiusSum * RadiusSum)
    {
        Result.bHit = true;
        // 두 구 중심 사이 중간점 (반지름 비율로 보간)
        float Dist = std::sqrt(DistSq);
        FVector Dir = (Dist > KINDA_SMALL_NUMBER)
                          ? (CenterB - CenterA) / Dist
                          : FVector(0, 0, 1);
        Result.HitPoint = CenterA + Dir * RadiusA;
    }

    return Result;
}

FCollisionResult FCollision::IntersectBoxSphere(const UBoxComponent* Box, const USphereComponent* Sphere)
{
    FCollisionResult Result;

    const FOBB OBB = Box->GetWorldOBB();

    FVector Center = Sphere->GetWorldLocation();
    float Radius = Sphere->GetScaledSphereRadius();

    FVector Closest = ClosestPointOnOBB(Center, OBB);

    float DistSq = (Center - Closest).SizeSquared();

    if (DistSq <= Radius * Radius)
    {
        Result.bHit = true;
        Result.HitPoint = Closest;
    }

    return Result;
}

FCollisionResult FCollision::IntersectCapsuleCapsule(const UCapsuleComponent* A, const UCapsuleComponent* B)
{
    FCollisionResult Result;

    FVector CenterA = A->GetWorldLocation();
    FVector CenterB = B->GetWorldLocation();

    float HalfA = A->GetScaledCapsuleHalfHeight();
    float HalfB = B->GetScaledCapsuleHalfHeight();

    float RadiusA = A->GetScaledCapsuleRadius();
    float RadiusB = B->GetScaledCapsuleRadius();

    FVector UpA = A->GetWorldTransform().GetUnitAxis(EAxis::Z);
    FVector UpB = B->GetWorldTransform().GetUnitAxis(EAxis::Z);

    FVector A0 = CenterA - UpA * HalfA;
    FVector A1 = CenterA + UpA * HalfA;

    FVector B0 = CenterB - UpB * HalfB;
    FVector B1 = CenterB + UpB * HalfB;

    float DistSq = SegmentSegmentDistSq(A0, A1, B0, B1);
    float RadiusSum = RadiusA + RadiusB;

    if (DistSq <= RadiusSum * RadiusSum)
    {
        Result.bHit = true;
        Result.HitPoint = (CenterA + CenterB) * 0.5f;
    }

    return Result;
}

FCollisionResult FCollision::IntersectCapsuleSphere(
    const UCapsuleComponent* A,
    const USphereComponent* B)
{
    FCollisionResult Result;

    FVector CenterA = A->GetWorldLocation();
    FVector UpA = A->GetWorldTransform().GetUnitAxis(EAxis::Z);
    float HalfA = A->GetScaledCapsuleHalfHeight();

    FVector A0 = CenterA - UpA * HalfA;
    FVector A1 = CenterA + UpA * HalfA;

    FVector P = B->GetWorldLocation();

    float RadiusA = A->GetScaledCapsuleRadius();
    float RadiusB = B->GetScaledSphereRadius();

    float DistSq = PointSegmentDistSq(P, A0, A1);

    float R = RadiusA + RadiusB;

    if (DistSq <= R * R)
    {
        Result.bHit = true;
        // 세그먼트 위 최근접점
        FVector AB = A1 - A0;
        float t = std::clamp((P - A0).DotProduct(AB) / AB.DotProduct(AB), 0.0f, 1.0f);
        Result.HitPoint = A0 + AB * t;
    }

    return Result;
}

FCollisionResult FCollision::IntersectCapsuleBox(
    const UCapsuleComponent* Cap,
    const UBoxComponent* Box)
{
    FCollisionResult Result;

    FVector C = Cap->GetWorldLocation();
    FVector Axis = Cap->GetWorldTransform().GetUnitAxis(EAxis::Z);
    float Half = Cap->GetScaledCapsuleHalfHeight();

    FVector A0 = C - Axis * Half;
    FVector A1 = C + Axis * Half;

    FOBB OBB = Box->GetWorldOBB();

    FVector C0 = ClosestPointOnOBB(A0, OBB);
    FVector C1 = ClosestPointOnOBB(A1, OBB);

    float Dist0 = PointSegmentDistSq(C0, A0, A1);
    float Dist1 = PointSegmentDistSq(C1, A0, A1);

    float DistSq = std::min(Dist0, Dist1);
    float R = Cap->GetScaledCapsuleRadius();

    if (DistSq <= R * R)
    {
        Result.bHit = true;
        Result.HitPoint = (Dist0 < Dist1) ? C0 : C1;
    }

    return Result;
}

FVector FCollision::ClosestOnBoxToSegment(FVector P0, FVector P1, const FOBB& Box)
{
    FVector C0 = ClosestPointOnOBB(P0, Box);
    FVector C1 = ClosestPointOnOBB(P1, Box);

    float d0 = (P0 - C0).SizeSquared();
    float d1 = (P1 - C1).SizeSquared();

    return (d0 < d1) ? C0 : C1;
}

float FCollision::PointSegmentDistSq(const FVector& P, const FVector& A, const FVector& B)
{
    FVector AB = B - A;
    FVector AP = P - A;

    float t = AP.DotProduct(AB) / AB.DotProduct(AB);
    t = std::clamp(t, 0.0f, 1.0f);

    FVector Closest = A + AB * t;
    return (P - Closest).SizeSquared();
}