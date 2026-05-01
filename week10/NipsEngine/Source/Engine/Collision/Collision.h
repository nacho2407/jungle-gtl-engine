#pragma once
#include "Geometry/AABB.h"
#include "Geometry/OBB.h"
#include <algorithm>

class UPrimitiveComponent;
class UBoxComponent;
class USphereComponent;
class UCapsuleComponent;

struct FCollision
{
    // Top-level
    static bool CheckOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B);

    static float SegmentSegmentDistSq(
        const FVector& P1, const FVector& Q1,
        const FVector& P2, const FVector& Q2);

    // Shape vs Shape
    static bool IntersectBoxBox(const UBoxComponent* A, const UBoxComponent* B);
    static bool IntersectOBB(const FOBB& A, const FOBB& B);
    static FVector ClosestPointOnOBB(const FVector& P, const FOBB& Box);

    static bool IntersectSphereSphere(const USphereComponent* A, const USphereComponent* B);
    static bool IntersectBoxSphere(const UBoxComponent* Box, const USphereComponent* Sphere);

    static bool IntersectCapsuleCapsule(const UCapsuleComponent* A, const UCapsuleComponent* B);
    static bool IntersectCapsuleSphere(const UCapsuleComponent* A, const USphereComponent* B);
    static bool IntersectCapsuleBox(const UCapsuleComponent* Cap, const UBoxComponent* Box);
    static FVector ClosestOnBoxToSegment(FVector P0, FVector P1, const FOBB& Box);
    static float PointSegmentDistSq(const FVector& P, const FVector& A, const FVector& B);
};