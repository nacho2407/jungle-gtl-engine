#pragma once
#include "Geometry/AABB.h"
#include "Geometry/OBB.h"
#include <algorithm>

class UPrimitiveComponent;
class UBoxComponent;
class USphereComponent;
class UCapsuleComponent;

struct FCollisionResult
{
    bool bHit = false;
    FVector HitPoint = FVector::ZeroVector;
    FVector HitNormal = FVector::ZeroVector;
};

struct FCollision
{
    // Top-level
    static FCollisionResult CheckOverlap(UPrimitiveComponent* A, UPrimitiveComponent* B);
    static float SegmentSegmentDistSq(
        const FVector& P1, const FVector& Q1,
        const FVector& P2, const FVector& Q2);

    // Shape vs Shape
    static FCollisionResult IntersectBoxBox(const UBoxComponent* A, const UBoxComponent* B);
    static FCollisionResult IntersectOBB(const FOBB& A, const FOBB& B);
    static FVector ClosestPointOnOBB(const FVector& P, const FOBB& Box);
    static FCollisionResult IntersectSphereSphere(const USphereComponent* A, const USphereComponent* B);
    static FCollisionResult IntersectBoxSphere(const UBoxComponent* Box, const USphereComponent* Sphere);
    static FCollisionResult IntersectCapsuleCapsule(const UCapsuleComponent* A, const UCapsuleComponent* B);
    static FCollisionResult IntersectCapsuleSphere(const UCapsuleComponent* A, const USphereComponent* B);
    static FCollisionResult IntersectCapsuleBox(const UCapsuleComponent* Cap, const UBoxComponent* Box);
    static FVector ClosestOnBoxToSegment(FVector P0, FVector P1, const FOBB& Box);
    static float PointSegmentDistSq(const FVector& P, const FVector& A, const FVector& B);
};