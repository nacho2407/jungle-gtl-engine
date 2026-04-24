// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Math/Matrix.h"

struct FBoundingBox;

// FOcclusionCulling는 렌더 영역의 핵심 동작을 담당합니다.
class FOcclusionCulling
{
public:
    void Clear();
    void RasterizeOccluder(const FBoundingBox& Box, const FMatrix& ViewProj);
    bool IsOccluded(const FBoundingBox& Box, const FMatrix& ViewProj);

private:
    static constexpr int BUF_W = 128;
    static constexpr int BUF_H = 72;

    float DepthBuffer[BUF_W * BUF_H];
};
