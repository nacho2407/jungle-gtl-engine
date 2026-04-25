#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Math/Matrix.h"

struct FBoundingBox;

// EAABBFrustumClassify는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class EAABBFrustumClassify : int
{
    Outside,
    Intersects,
    Contains,
};

// FConvexVolume는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FConvexVolume
{
public:
    void UpdateFromMatrix(const FMatrix& InViewProjectionMatrix);
    void UpdateAsOBB(const FMatrix& InWorldMatrix);
    bool IntersectAABB(const FBoundingBox& Box) const;
    // Returns true if the AABB is completely inside all 6 frustum planes
    bool ContainsAABB(const FBoundingBox& Box) const;

    EAABBFrustumClassify ClassifyAABB(const FBoundingBox& Box) const;

private:
    FVector4 Planes[6];
};
