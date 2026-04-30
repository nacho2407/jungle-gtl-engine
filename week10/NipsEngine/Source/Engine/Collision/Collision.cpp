#include "Collision.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "Core/CoreMinimal.h"
#include <algorithm>

#define KINDA_SMALL_NUMBER 1e-6

bool FCollision::CheckOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B)
{
    if (!A || !B)
        return false;

	/*
    if (!A->IsValid() || !B->IsValid())
        return false;
	*/

    if (!A->ShouldGenerateOverlapEvents() || !B->ShouldGenerateOverlapEvents())
        return false;

    auto TypeA = A->GetPrimitiveType();
    auto TypeB = B->GetPrimitiveType();

    // Box vs Box
    if (TypeA == EPrimitiveType::EPT_Box && TypeB == EPrimitiveType::EPT_Box)
        return IntersectBoxBox((UBoxComponent*)A, (UBoxComponent*)B);

    // Sphere vs Sphere
    if (TypeA == EPrimitiveType::EPT_Sphere && TypeB == EPrimitiveType::EPT_Sphere)
        return IntersectSphereSphere((USphereComponent*)A, (USphereComponent*)B);

    // Box vs Sphere
    if (TypeA == EPrimitiveType::EPT_Box && TypeB == EPrimitiveType::EPT_Sphere)
        return IntersectBoxSphere((UBoxComponent*)A, (USphereComponent*)B);

    if (TypeA == EPrimitiveType::EPT_Sphere && TypeB == EPrimitiveType::EPT_Box)
        return IntersectBoxSphere((UBoxComponent*)B, (USphereComponent*)A);

    // Capsule vs Capsule
    if (TypeA == EPrimitiveType::EPT_Capsule && TypeB == EPrimitiveType::EPT_Capsule)
        return IntersectCapsuleCapsule((UCapsuleComponent*)A, (UCapsuleComponent*)B);

    return false;
}

FVector FCollision::ClosestPointOnAABB(const FVector& P, const FAABB& AABB)
{
    return FVector(std::clamp(P.X, AABB.Min.X, AABB.Max.X), std::clamp(P.Y, AABB.Min.Y, AABB.Max.Y), std::clamp(P.Z, AABB.Min.Z, AABB.Max.Z));
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

bool FCollision::IntersectBoxBox(const UBoxComponent* A, const UBoxComponent* B)
{
    const FAABB& AABB_A = A->GetWorldAABB();
    const FAABB& AABB_B = B->GetWorldAABB();

    return (AABB_A.Min.X <= AABB_B.Max.X && AABB_A.Max.X >= AABB_B.Min.X) &&
           (AABB_A.Min.Y <= AABB_B.Max.Y && AABB_A.Max.Y >= AABB_B.Min.Y) &&
           (AABB_A.Min.Z <= AABB_B.Max.Z && AABB_A.Max.Z >= AABB_B.Min.Z);
}

bool FCollision::IntersectSphereSphere(const USphereComponent* A, const USphereComponent* B)
{
    FVector CenterA = A->GetWorldLocation();
    FVector CenterB = B->GetWorldLocation();

    float RadiusA = A->GetScaledSphereRadius();
    float RadiusB = B->GetScaledSphereRadius();

    float DistSq = (CenterA - CenterB).SizeSquared();
    float RadiusSum = RadiusA + RadiusB;

    return DistSq <= RadiusSum * RadiusSum;
}

bool FCollision::IntersectBoxSphere(const UBoxComponent* Box, const USphereComponent* Sphere)
{
    const FAABB& AABB = Box->GetWorldAABB();
    FVector Center = Sphere->GetWorldLocation();
    float Radius = Sphere->GetScaledSphereRadius();

    FVector Closest = ClosestPointOnAABB(Center, AABB);

    float DistSq = (Center - Closest).SizeSquared();

    return DistSq <= Radius * Radius;
}

bool FCollision::IntersectCapsuleCapsule(const UCapsuleComponent* A, const UCapsuleComponent* B)
{
    FVector CenterA = A->GetWorldLocation();
    FVector CenterB = B->GetWorldLocation();

    float HalfA = A->GetScaledCapsuleHalfHeight();
    float HalfB = B->GetScaledCapsuleHalfHeight();

    float RadiusA = A->GetScaledCapsuleRadius();
    float RadiusB = B->GetScaledCapsuleRadius();

    FVector Up = FVector(0, 0, 1);

    FVector A0 = CenterA - Up * HalfA;
    FVector A1 = CenterA + Up * HalfA;

    FVector B0 = CenterB - Up * HalfB;
    FVector B1 = CenterB + Up * HalfB;

    float DistSq = SegmentSegmentDistSq(A0, A1, B0, B1);
    float RadiusSum = RadiusA + RadiusB;

    return DistSq <= RadiusSum * RadiusSum;
}
