#pragma once
#include "Engine/Geometry/AABB.h"

class UPrimitiveComponent;
class UBoxComponent;
class USphereComponent;
class UCapsuleComponent;

struct FCollision
{
    // Top-level
    static bool CheckOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B);

    static FVector ClosestPointOnAABB(const FVector& P, const FAABB& AABB);
    static float SegmentSegmentDistSq(
        const FVector& P1, const FVector& Q1,
        const FVector& P2, const FVector& Q2);

    // Shape vs Shape
    static bool IntersectBoxBox(const UBoxComponent* A, const UBoxComponent* B);
    static bool IntersectSphereSphere(const USphereComponent* A, const USphereComponent* B);
    static bool IntersectBoxSphere(const UBoxComponent* Box, const USphereComponent* Sphere);

    static bool IntersectCapsuleCapsule(const UCapsuleComponent* A, const UCapsuleComponent* B);

};