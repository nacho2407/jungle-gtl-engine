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
Texture2D<float> HiZTextureA : register(t1);   // even mips (0, 2, 4, ...)
Texture2D<float> HiZTextureB : register(t2);   // odd  mips (1, 3, 5, ...)
RWStructuredBuffer<uint> VisibilityFlags : register(u0);

// Conservative margin: expand screen-space AABB by this many pixels
// to compensate for temporal delay (2-frame latency)
static const float SCREEN_EXPAND_PIXELS = 4.0;

// Depth bias: object must be this much farther than Hi-Z to be occluded
// prevents flickering at occlusion boundaries
static const float DEPTH_BIAS = 0.0005;

[numthreads(64, 1, 1)]
void CSOcclusionTest(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x;
    if (idx >= NumAABBs)
        return;

    AABB aabb = AABBs[idx];

    // 8 corners of the AABB
    float3 corners[8];
    corners[0] = float3(aabb.Min.x, aabb.Min.y, aabb.Min.z);
    corners[1] = float3(aabb.Max.x, aabb.Min.y, aabb.Min.z);
    corners[2] = float3(aabb.Min.x, aabb.Max.y, aabb.Min.z);
    corners[3] = float3(aabb.Max.x, aabb.Max.y, aabb.Min.z);
    corners[4] = float3(aabb.Min.x, aabb.Min.y, aabb.Max.z);
    corners[5] = float3(aabb.Max.x, aabb.Min.y, aabb.Max.z);
    corners[6] = float3(aabb.Min.x, aabb.Max.y, aabb.Max.z);
    corners[7] = float3(aabb.Max.x, aabb.Max.y, aabb.Max.z);

    float minDepth = 1.0;
    float2 minUV = float2(1.0, 1.0);
    float2 maxUV = float2(0.0, 0.0);
    bool anyFront = false;

    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float4 clip = mul(float4(corners[i], 1.0), ViewProj);

        if (clip.w > 0.0001)
        {
            anyFront = true;
            float3 ndc = clip.xyz / clip.w;

            // NDC → UV  (Y flipped)
            float2 uv = ndc.xy * float2(0.5, -0.5) + 0.5;

            minUV = min(minUV, uv);
            maxUV = max(maxUV, uv);
            minDepth = min(minDepth, ndc.z);
        }
    }

    // All corners behind camera → conservatively visible
    if (!anyFront)
    {
        VisibilityFlags[idx] = 1;
        return;
    }

    // Crosses near plane → visible
    if (minDepth < 0.0)
    {
        VisibilityFlags[idx] = 1;
        return;
    }

    // Expand screen-space AABB by margin (temporal compensation)
    float2 pixelSize = 1.0 / ViewportSize;
    float2 expand = pixelSize * SCREEN_EXPAND_PIXELS;
    minUV = saturate(minUV - expand);
    maxUV = saturate(maxUV + expand);

    // Screen-space pixel extent
    float2 screenSize = (maxUV - minUV) * ViewportSize;
    float maxDim = max(screenSize.x, screenSize.y);

    if (maxDim < 1.0)
    {
        // Sub-pixel → occluded (too small to matter)
        VisibilityFlags[idx] = 0;
        return;
    }

    // Select mip where AABB spans ~1-2 texels
    uint mipLevel = (uint) ceil(log2(maxDim));
    mipLevel = min(mipLevel, HiZMipCount - 1);

    // Mip dimensions
    uint vpW = (uint) ViewportSize.x;
    uint vpH = (uint) ViewportSize.y;
    uint mipW = max(vpW >> mipLevel, 1u);
    uint mipH = max(vpH >> mipLevel, 1u);

    // Texel coordinates
    int2 minTexel = int2(minUV * float2(mipW, mipH));
    int2 maxTexel = int2(maxUV * float2(mipW, mipH));
    minTexel = clamp(minTexel, int2(0, 0), int2(mipW - 1, mipH - 1));
    maxTexel = clamp(maxTexel, int2(0, 0), int2(mipW - 1, mipH - 1));

    // If too many texels, bump mip
    int2 range = maxTexel - minTexel + int2(1, 1);
    if (range.x > 4 || range.y > 4)
    {
        mipLevel = min(mipLevel + 1, HiZMipCount - 1);
        mipW = max(vpW >> mipLevel, 1u);
        mipH = max(vpH >> mipLevel, 1u);
        minTexel = int2(minUV * float2(mipW, mipH));
        maxTexel = int2(maxUV * float2(mipW, mipH));
        minTexel = clamp(minTexel, int2(0, 0), int2(mipW - 1, mipH - 1));
        maxTexel = clamp(maxTexel, int2(0, 0), int2(mipW - 1, mipH - 1));
    }

    // Sample Hi-Z: max depth in the covered region
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

    // Conservative occlusion: require depth bias margin to avoid flickering
    VisibilityFlags[idx] = (minDepth > maxHiZDepth + DEPTH_BIAS) ? 0 : 1;
}
