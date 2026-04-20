#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Math/Matrix.h"

struct FBoundingBox;

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