#pragma once
#include "Engine/Core/EngineTypes.h"
#include "OBB.h"
#include "Sphere.h"
#include "Serialization/Archive.h"

// 전방 선언
class FCollision;
class FSphereCollision;
class FBoxCollision;
class FCapsuleCollision;

/** 충돌 도형의 타입을 정의합니다. */
enum class ECollisionType
{
    Sphere,
    Box,
    Capsule
};

/** * 모든 충돌체의 최상위 인터페이스입니다.
 * 실제 수학적 데이터(Radius, Extent 등)와 교차 판정 로직을 분리하여 관리합니다.
 */
class FCollision
{
public:
    virtual ~FCollision() = default;

    virtual ECollisionType GetType() const = 0;

    /** 다형성을 이용한 메인 겹침 판정 함수 */
    virtual bool IsOverlapping(const FCollision* Other) const = 0;

    /** * 각 도형별 구체적인 충돌 체크 함수들 (이중 디스패치용)
     * 새로운 도형이 추가될 때마다 여기에 가상 함수를 추가합니다.
     */
    virtual bool IntersectWithSphere(const FSphereCollision& Sphere) const = 0;
    virtual bool IntersectWithBox(const FBoxCollision& Box) const = 0;
    virtual bool IntersectWithCapsule(const FCapsuleCollision& Capsule) const = 0;
};

// -----------------------------------------------------------------------------
// 1. 구체(Sphere) 충돌체
// -----------------------------------------------------------------------------
class FSphereCollision : public FCollision
{
public:
    FSphere Sphere;

    FSphereCollision(FSphere InSphere) : Sphere(InSphere) {}

    virtual ECollisionType GetType() const override { return ECollisionType::Sphere; }

    virtual bool IsOverlapping(const FCollision* Other) const override
    {
        return Other->IntersectWithSphere(*this);
    }

    virtual bool IntersectWithSphere(const FSphereCollision& Sphere) const override;
    virtual bool IntersectWithBox(const FBoxCollision& Box) const override;
    virtual bool IntersectWithCapsule(const FCapsuleCollision& Capsule) const override;

};
inline FArchive& operator<<(FArchive& Ar, FSphereCollision& Collision)
{

    Ar << Collision.Sphere;
    return Ar;
}
// -----------------------------------------------------------------------------
// 2. 박스(Box/AABB) 충돌체
// -----------------------------------------------------------------------------
class FBoxCollision : public FCollision
{
public:
    FOBB Bounds; // Min, Max 좌표를 가집니다.

    FBoxCollision(FOBB InBounds) : Bounds(InBounds) {}

    virtual ECollisionType GetType() const override { return ECollisionType::Box; }

    virtual bool IsOverlapping(const FCollision* Other) const override
    {
        return Other->IntersectWithBox(*this);
    }

    virtual bool IntersectWithSphere(const FSphereCollision& Sphere) const override;
    virtual bool IntersectWithBox(const FBoxCollision& Box) const override;
    virtual bool IntersectWithCapsule(const FCapsuleCollision& Capsule) const override;
};
inline FArchive& operator<<(FArchive& Ar, FBoxCollision& Collision)
{
    Ar << Collision.Bounds;
    return Ar;
}
// -----------------------------------------------------------------------------
// 3. 캡슐(Capsule) 충돌체
// -----------------------------------------------------------------------------
class FCapsuleCollision : public FCollision
{
public:
    FVector Center;
    float HalfHeight; // 캡슐 중심에서 양 끝 원의 중심까지의 거리
    float Radius;     // 캡슐의 반지름
    FVector UpVector; // 캡슐이 서 있는 방향

    FCapsuleCollision(FVector InCenter, float InHalfHeight, float InRadius, FVector InUp = FVector(0, 0, 1))
        : Center(InCenter), HalfHeight(InHalfHeight), Radius(InRadius), UpVector(InUp) {}

    virtual ECollisionType GetType() const override { return ECollisionType::Capsule; }

    virtual bool IsOverlapping(const FCollision* Other) const override
    {
        return Other->IntersectWithCapsule(*this);
    }

    virtual bool IntersectWithSphere(const FSphereCollision& Sphere) const override;
    virtual bool IntersectWithBox(const FBoxCollision& Box) const override;
    virtual bool IntersectWithCapsule(const FCapsuleCollision& Capsule) const override;
};

inline FArchive& operator<<(FArchive& Ar, FCapsuleCollision& Collision)
{
    Ar << Collision.Center;
    Ar << Collision.HalfHeight;
    Ar << Collision.Radius;
    Ar << Collision.UpVector;
    return Ar;
}