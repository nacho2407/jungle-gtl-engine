#pragma once

#include "Core/CoreMinimal.h"

struct FTrackedMemoryStats
{
    uint64 RenderTargetBytes = 0;
    uint64 DepthStencilBytes = 0;
    uint64 VertexBufferBytes = 0;
    uint64 IndexBufferBytes = 0;
    uint64 VertexShaderBlobBytes = 0;
    uint64 PixelShaderBlobBytes = 0;

    uint64 TotalTrackedGpuBytes() const
    {
        return RenderTargetBytes + DepthStencilBytes + VertexBufferBytes + IndexBufferBytes +
               VertexShaderBlobBytes + PixelShaderBlobBytes;
    }
};
