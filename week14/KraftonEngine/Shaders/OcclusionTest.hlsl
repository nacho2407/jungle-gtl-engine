// GPU Occlusion Test — AABB visibility against Hi-Z mip chain
// Dispatched with ceil(NumAABBs / 64) groups

#pragma pack_matrix(row_major)

struct AABB
{
    float3 Min;
    float _pad0;
    float3 Max;
    float _pad1;
};

cbuffer OcclusionParams : register(b0)
{
    float4x4 ViewProj;
    float2 ViewportSize;
    uint NumAABBs;
    uint HiZMipCount;
};

StructuredBuffer<AABB> AABBs : register(t0);
Texture2D<float> HiZTextureA : register(t1); // even mips (0, 2, 4, ...)
Texture2D<float> HiZTextureB : register(t2); // odd  mips (1, 3, 5, ...)
RWStructuredBuffer<uint> VisibilityFlags : register(u0);

// Depth bias: vertex must be this much farther than Hi-Z to be occluded
// prevents flickering at occlusion boundaries
static const float DEPTH_BIAS = 0.0005;

[numthreads(64, 1, 1)]
void CSOcclusionTest(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x;
    if (idx >= NumAABBs)
        return;

    AABB aabb = AABBs[idx];
    uint vpW = (uint) ViewportSize.x;
    uint vpH = (uint) ViewportSize.y;

    // ════════════════════════════════════════════════
    // Stage 1: Bounding Sphere quick-reject
    // ════════════════════════════════════════════════
    float3 center = (aabb.Min + aabb.Max) * 0.5;
    float radius = length(aabb.Max - center);

    float4 centerClip = mul(float4(center, 1.0), ViewProj);

    if (centerClip.w > 0.0001)
    {
        float centerDepth = centerClip.z / centerClip.w;
        float2 centerUV = centerClip.xy / centerClip.w * float2(0.5, -0.5) + 0.5;

        // Approximate sphere depth extent in NDC (linear approximation)
        float depthExtent = radius / centerClip.w;
        float nearDepth = centerDepth - depthExtent;

        // Screen-space radius for sub-pixel check and mip selection
        float screenRadius = radius / centerClip.w * max(ViewportSize.x, ViewportSize.y) * 0.5;

        // Sub-pixel sphere → occluded
        if (screenRadius < 0.5)
        {
            VisibilityFlags[idx] = 0;
            return;
        }

        // Quick-reject: sphere's nearest depth vs Hi-Z at covering mip
        // If sphere is entirely on-screen and fully in front of near plane
        if (nearDepth > 0.0
            && centerUV.x >= 0.0 && centerUV.x <= 1.0
            && centerUV.y >= 0.0 && centerUV.y <= 1.0)
        {
            uint mip = (uint) ceil(log2(max(screenRadius * 2.0, 1.0)));
            mip = min(mip, HiZMipCount - 1);
            uint mW = max(vpW >> mip, 1u);
            uint mH = max(vpH >> mip, 1u);
            int2 texel = clamp(int2(centerUV * float2(mW, mH)), int2(0, 0), int2(mW - 1, mH - 1));
            float hiZ = (mip & 1)
                ? HiZTextureB.Load(int3(texel, mip))
                : HiZTextureA.Load(int3(texel, mip));

            // Sphere's nearest point is behind the farthest occluder → definitely occluded
            if (nearDepth > hiZ + DEPTH_BIAS)
            {
                VisibilityFlags[idx] = 0;
                return;
            }
        }
    }

    // ════════════════════════════════════════════════
    // Stage 2: Per-vertex precision test
    // ════════════════════════════════════════════════
    float3 corners[8];
    corners[0] = float3(aabb.Min.x, aabb.Min.y, aabb.Min.z);
    corners[1] = float3(aabb.Max.x, aabb.Min.y, aabb.Min.z);
    corners[2] = float3(aabb.Min.x, aabb.Max.y, aabb.Min.z);
    corners[3] = float3(aabb.Max.x, aabb.Max.y, aabb.Min.z);
    corners[4] = float3(aabb.Min.x, aabb.Min.y, aabb.Max.z);
    corners[5] = float3(aabb.Max.x, aabb.Min.y, aabb.Max.z);
    corners[6] = float3(aabb.Min.x, aabb.Max.y, aabb.Max.z);
    corners[7] = float3(aabb.Max.x, aabb.Max.y, aabb.Max.z);

    // Project each corner: collect screen UV, depth, and screen-space AABB
    float2 minUV = float2(1.0, 1.0);
    float2 maxUV = float2(0.0, 0.0);
    float minDepth = 1.0;
    float2 projUV[8];
    float projDepth[8];
    bool onScreen[8];
    bool anyFront = false;
    bool anyBehind = false;
    int onScreenCount = 0;

    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float4 clip = mul(float4(corners[i], 1.0), ViewProj);

        if (clip.w <= 0.0001)
        {
            anyBehind = true;
            onScreen[i] = false;
            continue;
        }

        anyFront = true;
        float3 ndc = clip.xyz / clip.w;
        float2 uv = ndc.xy * float2(0.5, -0.5) + 0.5;

        projUV[i] = uv;
        projDepth[i] = ndc.z;

        // Accumulate screen-space AABB (unclamped for rect coverage)
        minUV = min(minUV, uv);
        maxUV = max(maxUV, uv);
        minDepth = min(minDepth, ndc.z);

        onScreen[i] = (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0);
        if (onScreen[i])
            onScreenCount++;
    }

    // Behind camera or straddling near plane → conservatively visible
    if (!anyFront || anyBehind)
    {
        VisibilityFlags[idx] = 1;
        return;
    }

    // Near plane intersection → visible
    if (minDepth < 0.0)
    {
        VisibilityFlags[idx] = 1;
        return;
    }

    // All corners off-screen → straddles viewport → visible
    if (onScreenCount == 0)
    {
        VisibilityFlags[idx] = 1;
        return;
    }

    // ── Per-vertex test: single texel at mip 0 (8 Load, early-out on first visible) ──
    [unroll]
    for (int j = 0; j < 8; j++)
    {
        if (!onScreen[j])
            continue;

        int2 texel = int2(projUV[j] * float2(vpW, vpH));
        texel = clamp(texel, int2(0, 0), int2(vpW - 1, vpH - 1));
        float hiZ = HiZTextureA.Load(int3(texel, 0));

        if (projDepth[j] <= hiZ + DEPTH_BIAS)
        {
            VisibilityFlags[idx] = 1;
            return;
        }
    }

    // ════════════════════════════════════════════════
    // Stage 3: Screen rect coverage test
    // Catches visibility between corners (surface visible but all corners occluded)
    // ════════════════════════════════════════════════
    float2 clampedMin = saturate(minUV);
    float2 clampedMax = saturate(maxUV);

    // Use unclamped extent for mip selection (handles partial off-screen correctly)
    float2 screenSize = (maxUV - minUV) * ViewportSize;
    float maxDim = max(screenSize.x, screenSize.y);

    // Large screen coverage → too expensive to test, conservatively visible
    if (maxDim > ViewportSize.x * 0.75 || maxDim > ViewportSize.y * 0.75)
    {
        VisibilityFlags[idx] = 1;
        return;
    }

    // Select mip where AABB spans ~2-4 texels
    uint mipLevel = (uint) ceil(log2(max(maxDim, 1.0)));
    mipLevel = min(mipLevel, HiZMipCount - 1);

    uint mipW = max(vpW >> mipLevel, 1u);
    uint mipH = max(vpH >> mipLevel, 1u);

    int2 minTexel = clamp(int2(clampedMin * float2(mipW, mipH)), int2(0, 0), int2(mipW - 1, mipH - 1));
    int2 maxTexel = clamp(int2(clampedMax * float2(mipW, mipH)), int2(0, 0), int2(mipW - 1, mipH - 1));

    // If too many texels, bump mip
    int2 range = maxTexel - minTexel + int2(1, 1);
    if (range.x > 4 || range.y > 4)
    {
        mipLevel = min(mipLevel + 1, HiZMipCount - 1);
        mipW = max(vpW >> mipLevel, 1u);
        mipH = max(vpH >> mipLevel, 1u);
        minTexel = clamp(int2(clampedMin * float2(mipW, mipH)), int2(0, 0), int2(mipW - 1, mipH - 1));
        maxTexel = clamp(int2(clampedMax * float2(mipW, mipH)), int2(0, 0), int2(mipW - 1, mipH - 1));
    }

    // Sample Hi-Z rect: if max depth in region >= minDepth → visible
    float maxHiZDepth = 0.0;
    for (int y = minTexel.y; y <= maxTexel.y; y++)
    {
        for (int x = minTexel.x; x <= maxTexel.x; x++)
        {
            float d = (mipLevel & 1)
                ? HiZTextureB.Load(int3(x, y, mipLevel))
                : HiZTextureA.Load(int3(x, y, mipLevel));
            maxHiZDepth = max(maxHiZDepth, d);
        }
    }

    VisibilityFlags[idx] = (minDepth > maxHiZDepth + DEPTH_BIAS) ? 0 : 1;
}
