#include "Engine/Collision/CollisionSystem.h"
#include "Math/Intersection.h"

// ============================================================================
// 1. FSphereCollision 구현
// ============================================================================

bool FSphereCollision::IntersectWithSphere(const FSphereCollision& Other) const
{
    // 구 vs 구: 두 중심 사이의 거리의 제곱이 두 반지름의 합의 제곱보다 작거나 같은지 확인
    FVector Diff = Sphere.Center - Other.Sphere.Center;
    float DistanceSq = Diff.Dot(Diff);

    float RadiusSum = Sphere.Radius + Other.Sphere.Radius;
    return DistanceSq <= (RadiusSum * RadiusSum);
}

bool FSphereCollision::IntersectWithBox(const FBoxCollision& Box) const
{
    return FMath::IntersectSphereOBB(
        Sphere.Center, Sphere.Radius,
        Box.Bounds.Center, Box.Bounds.Extent, Box.Bounds.Rotation);
}

bool FSphereCollision::IntersectWithCapsule(const FCapsuleCollision& Capsule) const
{
    // 구 vs 캡슐: 캡슐의 내부 선분(Segment)에서 구의 중심까지의 '최단 거리'를 구합니다.
    FVector CapsuleStart = Capsule.Center + Capsule.UpVector * Capsule.HalfHeight;
    FVector CapsuleEnd = Capsule.Center - Capsule.UpVector * Capsule.HalfHeight;

    // 선분과 점 사이의 가장 가까운 점을 찾는 함수
    FVector ClosestPoint = FMath::ClosestPointOnSegment(Sphere.Center, CapsuleStart, CapsuleEnd);

    FVector Diff = Sphere.Center - ClosestPoint;
    float DistanceSq = Diff.Dot(Diff);
    float RadiusSum = Sphere.Radius + Capsule.Radius;

    return DistanceSq <= (RadiusSum * RadiusSum);
}

// ============================================================================
// 2. FBoxCollision 구현
// ============================================================================

bool FBoxCollision::IntersectWithSphere(const FSphereCollision& Sphere) const
{
    return Sphere.IntersectWithBox(*this);
}

bool FBoxCollision::IntersectWithBox(const FBoxCollision& Other) const
{
    return FMath::IntersectOBBOBB(
        Bounds.Center, Bounds.Extent, Bounds.Rotation,
        Other.Bounds.Center, Other.Bounds.Extent, Other.Bounds.Rotation);
}

bool FBoxCollision::IntersectWithCapsule(const FCapsuleCollision& Capsule) const
{
    // OBB vs Capsule: OBB와 캡슐 선분 사이의 최단 거리를 구하거나 SAT를 응용합니다.
    return FMath::IntersectOBBCapsule(
        Bounds.Center, Bounds.Extent, Bounds.Rotation,
        Capsule.Center, Capsule.HalfHeight, Capsule.Radius, Capsule.UpVector);
}

// ============================================================================
// 3. FCapsuleCollision 구현
// ============================================================================

bool FCapsuleCollision::IntersectWithSphere(const FSphereCollision& Sphere) const
{
    return Sphere.IntersectWithCapsule(*this);
}

bool FCapsuleCollision::IntersectWithBox(const FBoxCollision& Box) const
{
    return Box.IntersectWithCapsule(*this);
}

bool FCapsuleCollision::IntersectWithCapsule(const FCapsuleCollision& Other) const
{
    // 캡슐 vs 캡슐: 두 캡슐의 '중심 선분' 사이의 가장 가까운 최단 거리를 구합니다.
    FVector AStart = Center + UpVector * HalfHeight;
    FVector AEnd = Center - UpVector * HalfHeight;

    FVector BStart = Other.Center + Other.UpVector * Other.HalfHeight;
    FVector BEnd = Other.Center - Other.UpVector * Other.HalfHeight;

    FVector ClosestPointA, ClosestPointB;
    // 두 3D 선분(Segment) 사이의 가장 가까운 두 점을 계산하는 함수
    FMath::ClosestPointsBetweenSegments(AStart, AEnd, BStart, BEnd, ClosestPointA, ClosestPointB);

    FVector Diff = ClosestPointA - ClosestPointB;
    float DistanceSq = Diff.Dot(Diff);
    float RadiusSum = Radius + Other.Radius;

    return DistanceSq <= (RadiusSum * RadiusSum);
}