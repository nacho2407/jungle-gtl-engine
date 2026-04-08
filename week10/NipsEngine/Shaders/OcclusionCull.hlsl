/*
    OcclusionCull.hlsl
    Approximate each object with a camera-facing bounding sphere and test it
    against mip0 depth to avoid false culling through small gaps.
*/

struct FGPUOcclusionAABB
{
    float3 Min;
    float  Pad0;
    float3 Max;
    float  Pad1;
};

cbuffer OcclusionCullConstants : register(b0)
{
    row_major float4x4 ViewProjection;
    float3 CameraForward;
    float ViewportWidth;
    float3 CameraRight;
    float ViewportHeight;
    float3 CameraUp;
    float ViewportMinX;
    float3 CameraPosition;
    float ViewportMinY;
    float TargetWidth;
    float TargetHeight;
    float HiZMipCount;
    uint  ObjectCount;
    float3 Padding0;
};

StructuredBuffer<FGPUOcclusionAABB> AABBs : register(t0);
Texture2D<float>                    HiZ   : register(t1);

RWStructuredBuffer<uint> Result : register(u0);

[numthreads(64, 1, 1)]
void OcclusionCullCS(uint3 DTid : SV_DispatchThreadID)
{
    const uint Index = DTid.x;
    if (Index >= ObjectCount)
    {
        return;
    }

    const FGPUOcclusionAABB Box = AABBs[Index];

    const float3 Center = (Box.Min + Box.Max) * 0.5f;
    const float3 Extents = (Box.Max - Box.Min) * 0.5f;
    const float Radius = max(length(Extents), 1e-4f);

    const float3 SamplePoints[7] =
    {
        Center,
        Center - CameraForward * Radius,
        Center + CameraForward * Radius,
        Center + CameraRight * Radius,
        Center - CameraRight * Radius,
        Center + CameraUp * Radius,
        Center - CameraUp * Radius
    };

    float maxNDCz = 0.0f;
    float minU = 1.0f;
    float minV = 1.0f;
    float maxU = 0.0f;
    float maxV = 0.0f;
    bool bAllBehindCamera = true;
    bool bAnyCloserThanNearPlane = false;
    bool bAnyWithinOrBeyondNearPlane = false;

    [unroll]
    for (int i = 0; i < 7; ++i)
    {
        const float4 Clip = mul(float4(SamplePoints[i], 1.0f), ViewProjection);

        bAllBehindCamera = bAllBehindCamera && (Clip.w <= 0.0f);
        bAnyCloserThanNearPlane = bAnyCloserThanNearPlane || (Clip.z > Clip.w);
        bAnyWithinOrBeyondNearPlane = bAnyWithinOrBeyondNearPlane || (Clip.z <= Clip.w);

        const float invW = 1.0f / max(abs(Clip.w), 1e-6f);
        const float ndcX = Clip.x * invW;
        const float ndcY = Clip.y * invW;
        const float ndcZ = Clip.z * invW;

        const float u = ndcX * 0.5f + 0.5f;
        const float v = 1.0f - (ndcY * 0.5f + 0.5f);

        minU = min(minU, u);
        maxU = max(maxU, u);
        minV = min(minV, v);
        maxV = max(maxV, v);
        maxNDCz = max(maxNDCz, ndcZ);
    }

    if (bAnyCloserThanNearPlane && bAnyWithinOrBeyondNearPlane)
    {
        Result[Index] = 1u;
        return;
    }

    if (bAllBehindCamera)
    {
        Result[Index] = 0u;
        return;
    }

    if (maxU < 0.0f || minU > 1.0f || maxV < 0.0f || minV > 1.0f)
    {
        Result[Index] = 0u;
        return;
    }

    minU = saturate(minU);
    maxU = saturate(maxU);
    minV = saturate(minV);
    maxV = saturate(maxV);

    const float projectedWidthPixels = (maxU - minU) * ViewportWidth;
    const float projectedHeightPixels = (maxV - minV) * ViewportHeight;
    const float projectedDiameterPixels = max(projectedWidthPixels, projectedHeightPixels);
    const float viewportShortEdge = min(ViewportWidth, ViewportHeight);
    const float cameraDistance = length(Center - CameraPosition);
    const bool bUseStrongNearGapProtection =
        (cameraDistance <= Radius * 18.0f) || (projectedDiameterPixels >= 32.0f);
    const bool bUseMediumGapProtection =
        bUseStrongNearGapProtection ||
        (cameraDistance <= Radius * 26.0f) || (projectedDiameterPixels >= 18.0f);

    // Expand the tested footprint more aggressively only for close or medium objects.
    const float footprintPadPixels = bUseStrongNearGapProtection ? 3.0f
        : (bUseMediumGapProtection ? 2.0f : 1.0f);
    const float padU = footprintPadPixels / max(ViewportWidth, 1.0f);
    const float padV = footprintPadPixels / max(ViewportHeight, 1.0f);
    minU = saturate(minU - padU);
    maxU = saturate(maxU + padU);
    minV = saturate(minV - padV);
    maxV = saturate(maxV + padV);

    // Small projected objects are prone to false occlusion through thin gaps, but
    // only keep them automatically when they are also near the camera.
    if (projectedDiameterPixels <= 22.0f && cameraDistance <= Radius * 14.0f)
    {
        Result[Index] = 1u;
        return;
    }

    // Very near or screen-dominant objects are the most likely to false-cull when
    // the camera gets close, so keep them visible as a conservative fallback.
    if (cameraDistance <= Radius * 3.5f || maxNDCz >= 0.84f ||
        projectedDiameterPixels >= viewportShortEdge * 0.20f)
    {
        Result[Index] = 1u;
        return;
    }

    const int mipI = 0;

    const int mipW = max(1, (int)(TargetWidth) >> mipI);
    const int mipH = max(1, (int)(TargetHeight) >> mipI);

    uint visibleSamples = 0u;
    const float depthBias = bUseStrongNearGapProtection ? 5.0e-4f
        : (bUseMediumGapProtection ? 3.5e-4f : 2.5e-4f);
    [unroll]
    for (int sy = 0; sy < 9; ++sy)
    {
        const float fy = ((float)sy + 0.5f) / 9.0f;
        const float sampleV = lerp(minV, maxV, fy);

        [unroll]
        for (int sx = 0; sx < 9; ++sx)
        {
            const float fx = ((float)sx + 0.5f) / 9.0f;
            const float sampleU = lerp(minU, maxU, fx);

            const float2 targetPixel = float2(ViewportMinX + sampleU * ViewportWidth,
                                              ViewportMinY + sampleV * ViewportHeight);
            const int2 coord = int2(clamp((int)targetPixel.x, 0, mipW - 1),
                                    clamp((int)targetPixel.y, 0, mipH - 1));
            const float sampleHiZ = HiZ.Load(int3(coord, mipI));
            if (maxNDCz >= sampleHiZ - depthBias)
            {
                ++visibleSamples;
            }
        }
    }

    uint requiredVisibleSamples = 1u;
    if (bUseMediumGapProtection && !bUseStrongNearGapProtection)
    {
        requiredVisibleSamples = (projectedDiameterPixels <= 24.0f) ? 2u : 1u;
    }
    else if (!bUseMediumGapProtection)
    {
        requiredVisibleSamples = (projectedDiameterPixels <= 24.0f)
            ? 6u
            : ((projectedDiameterPixels <= 48.0f) ? 4u : 3u);
    }

    if (visibleSamples >= requiredVisibleSamples)
    {
        Result[Index] = 1u;
        return;
    }

    // Close thin slits are still the hardest case, so only the near-protection
    // path pays for an even denser rescue pass.
    if (bUseStrongNearGapProtection &&
        projectedDiameterPixels <= 160.0f &&
        cameraDistance <= Radius * 20.0f)
    {
        uint conservativeVisibleSamples = visibleSamples;

        [unroll]
        for (int sy = 0; sy < 15; ++sy)
        {
            const float fy = ((float)sy + 0.5f) / 15.0f;
            const float sampleV = lerp(minV, maxV, fy);

            [unroll]
            for (int sx = 0; sx < 15; ++sx)
            {
                const float fx = ((float)sx + 0.5f) / 15.0f;
                const float sampleU = lerp(minU, maxU, fx);
                const float2 targetPixel = float2(ViewportMinX + sampleU * ViewportWidth,
                                                  ViewportMinY + sampleV * ViewportHeight);
                const int2 coord = int2(clamp((int)targetPixel.x, 0, mipW - 1),
                                        clamp((int)targetPixel.y, 0, mipH - 1));
                const float sampleHiZ = HiZ.Load(int3(coord, mipI));
                if (maxNDCz >= sampleHiZ - 9.0e-4f)
                {
                    ++conservativeVisibleSamples;
                }
            }
        }

        if (conservativeVisibleSamples > 0u)
        {
            Result[Index] = 1u;
            return;
        }
    }
    else if (bUseMediumGapProtection &&
             projectedDiameterPixels <= 112.0f &&
             cameraDistance <= Radius * 30.0f)
    {
        uint conservativeVisibleSamples = visibleSamples;

        [unroll]
        for (int sy = 0; sy < 11; ++sy)
        {
            const float fy = ((float)sy + 0.5f) / 11.0f;
            const float sampleV = lerp(minV, maxV, fy);

            [unroll]
            for (int sx = 0; sx < 11; ++sx)
            {
                const float fx = ((float)sx + 0.5f) / 11.0f;
                const float sampleU = lerp(minU, maxU, fx);
                const float2 targetPixel = float2(ViewportMinX + sampleU * ViewportWidth,
                                                  ViewportMinY + sampleV * ViewportHeight);
                const int2 coord = int2(clamp((int)targetPixel.x, 0, mipW - 1),
                                        clamp((int)targetPixel.y, 0, mipH - 1));
                const float sampleHiZ = HiZ.Load(int3(coord, mipI));
                if (maxNDCz >= sampleHiZ - 7.5e-4f)
                {
                    ++conservativeVisibleSamples;
                }
            }
        }

        if (conservativeVisibleSamples >= 2u)
        {
            Result[Index] = 1u;
            return;
        }
    }

    Result[Index] = 0u;
}
