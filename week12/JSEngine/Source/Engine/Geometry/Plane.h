#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Math/Utils.h"

struct FPlane
{
public:
    FVector Normal;
    FVector AbsNormal;
    float   D{0.0f};

public:
    FPlane();
    FPlane(const FVector& InNormal, float InD);
    FPlane(const FVector& InNormal, const FVector& PointOnPlane);
    FPlane(const FVector& PointA, const FVector& PointB, const FVector& PointC);

    float GetSignedDistanceToPoint(const FVector& Point) const;
    float GetAbsDistanceToPoint(const FVector& Point) const;

    bool Normalize(float Tolerance = MathUtil::Epsilon);
    FPlane GetNormalized(float Tolerance = MathUtil::Epsilon) const;

    void Flip();
    bool IsValid(float Tolerance = MathUtil::Epsilon) const;

private:
    void UpdateAbsNormal();
};
