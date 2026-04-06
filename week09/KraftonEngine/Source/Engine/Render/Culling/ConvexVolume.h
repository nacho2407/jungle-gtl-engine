#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Math/Vector.h"
#include "Engine/Math/Matrix.h"

struct FBoundingBox;

enum class EAABBResult
{
    Outside,
    Intersects,
    Contains
};

struct FPlaneWithMask
{
    FVector4 Plane;

    // p-vertex 선택용 부호 캐시
    // true  -> Box.Max 사용
    // false -> Box.Min 사용
    bool bPX = false;
    bool bPY = false;
    bool bPZ = false;
};

struct FConvexVolume
{
public:
    static constexpr uint32 PlaneCount = 6;

	void UpdateFromMatrix(const FMatrix& InViewProjectionMatrix);

    bool IntersectAABB(const FBoundingBox& Box) const;
    bool ContainsAABB(const FBoundingBox& Box) const;
    EAABBResult ClassifyAABB(const FBoundingBox& Box) const;

private:
    static FPlaneWithMask MakePlaneWithMask(const FVector4& InPlane);
private:
    FPlaneWithMask CachedPlanes[PlaneCount] = {};
};
