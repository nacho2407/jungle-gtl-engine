#pragma once
#pragma once

#include "Engine/Math/Vector.h"

struct FRay;
struct FMatrix;

struct FAABB
{
    FVector Min;
    FVector Max;

    FAABB();
    FAABB(const FVector& InMin, const FVector& InMax);

    void Reset();
    bool IsValid() const;

    void Expand(const FVector& Point);
    void Merge(const FAABB& Other);

    inline FVector GetCenter() const
    {
        // Original:
        // return (Min + Max) * 0.5f;

        const XMVector Center =
            DirectX::XMVectorScale(DirectX::XMVectorAdd(Min.ToXMVector(), Max.ToXMVector()), 0.5f);
        return FVector(Center);
    }
    inline FVector GetExtent() const
    {
        // Original:
        // return (Max - Min) * 0.5f;

        const XMVector Extent = DirectX::XMVectorScale(
            DirectX::XMVectorSubtract(Max.ToXMVector(), Min.ToXMVector()), 0.5f);
        return FVector(Extent);
    }

    bool         IntersectRay(const FRay& Ray, float& OutT) const;
    bool         IntersectRay(const FRay& Ray, float& OutTMin, float& OutTMax) const;
    static FAABB TransformAABB(const FAABB& InLocalAABB, const FMatrix& InMatrix);
    void         ExpandToInclude(const FAABB& Other);
};
