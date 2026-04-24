

/*
    LightCullingCommon.hlsl는 컬링/가시성 계산에 쓰는 셰이더입니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
    - 이 파일에서 직접 선언한 슬롯: b2, t6, t1, u1, u2, u3
*/

#ifndef LIGHT_CULLING_COMMON_HLSL
#define LIGHT_CULLING_COMMON_HLSL

#include "../../Common/Resources/ConstantBuffers.hlsl"


// ============================================================
// Defines
// ============================================================
#define TILE_SIZE                       4
#define NUM_SLICES                      32
#define MAX_LIGHTS_PER_TILE             1024
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32)   // 32

// ============================================================
// Structs
// ============================================================


struct FSphere
{
    float3 position;
    float  radius;
};

struct FPlane
{
    float3 normal;
    float  distance;
};

struct FFrustum
{
    FPlane planes[4];
};

// ============================================================
// Constant Buffer (b2)
// ============================================================
cbuffer LightCullingParams : register(b2)
{
    uint2  ScreenSize;
    uint2  TileSize;        // == TILE_SIZE

    uint   Enable25DCulling;
    float  NearZ;
    float  FarZ;
    float  NumLights;
};

// ============================================================
// Groupshared
// ============================================================
groupshared uint tileDepthMask;
groupshared uint groupMinZ;
groupshared uint groupMaxZ;
groupshared uint hitCount;

// ============================================================
// Resources
// ============================================================
StructuredBuffer<FLocalLightInfo> g_LightBuffer : register(t6);
Texture2D<float>              SceneDepth    : register(t1);

RWStructuredBuffer<uint>  PerTilePointLightIndexMaskOut : register(u1);
RWStructuredBuffer<uint>  CulledPointLightIndexMaskOUT  : register(u2);
RWTexture2D<float4>       DebugHitMap                   : register(u3);

// ============================================================
// Plane / Frustum
// ============================================================
FPlane ComputePlane(float3 p0, float3 p1, float3 p2)
{
    FPlane plane;
    float3 v0 = p1 - p0;
    float3 v1 = p2 - p0;
    plane.normal   = normalize(cross(v0, v1));
    plane.distance = dot(plane.normal, p0);
    return plane;
}

bool SphereInsidePlane(FSphere sphere, FPlane plane)
{
    return dot(plane.normal, sphere.position) - plane.distance < -sphere.radius;
}

bool SphereInsideFrustum(FSphere sphere, FFrustum frustum, float zNear, float zFar)
{
    if (sphere.position.z + sphere.radius < zNear ||
        sphere.position.z - sphere.radius > zFar)
        return false;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (SphereInsidePlane(sphere, frustum.planes[i]))
            return false;
    }
    return true;
}

// ============================================================
// 2.5D Depth Masking
// ============================================================

bool ShouldLightAffectTile(float3 lightVSPos, float radius, float minZ, float maxZ, uint depthMask)
{
    if (depthMask == 0)
        return false;

    float s_minDepth = lightVSPos.z - radius;
    float s_maxDepth = lightVSPos.z + radius;

    if (s_maxDepth < minZ || s_minDepth > maxZ)
        return false;

    float rangeZ = max(1e-5, maxZ - minZ);
    float normMin = saturate((s_minDepth - minZ) / rangeZ);
    float normMax = saturate((s_maxDepth - minZ) / rangeZ);

    int sliceMin = clamp((int) floor(normMin * NUM_SLICES), 0, NUM_SLICES - 1);
    int sliceMax = clamp((int) ceil (normMax * NUM_SLICES), 0, NUM_SLICES - 1);

    uint sphereMask = 0;
    [unroll]
    for (int i = sliceMin; i <= sliceMax; ++i)
        sphereMask |= (1u << i);

    return (sphereMask & depthMask) != 0;
}

// ============================================================
// ============================================================
void CullLight(
    uint                      index,
    float3                    lightVSPos,
    float                     radius,
    FFrustum                  frustum,
    float                     minZ,
    float                     maxZ,
    uint                      flatTileIndex,
    RWStructuredBuffer<uint>  MaskBuffer,
    RWStructuredBuffer<uint>  CulledMaskBuffer)
{
    FSphere s = { lightVSPos, radius };

    if (!SphereInsideFrustum(s, frustum, NearZ, FarZ))
        return;

    if (Enable25DCulling != 0 &&
        !ShouldLightAffectTile(lightVSPos, radius, minZ, maxZ, tileDepthMask))
        return;

    uint bucketIdx = index / 32;
    uint bitIdx    = index % 32;

    InterlockedOr(MaskBuffer[flatTileIndex * SHADER_ENTITY_TILE_BUCKET_COUNT + bucketIdx], 1u << bitIdx);
    InterlockedOr(CulledMaskBuffer[bucketIdx], 1u << bitIdx);

    InterlockedAdd(hitCount, 1);
}

// ============================================================
// ============================================================
float4 HeatColor(float t)
{
    float3 color = float3(0, 0, 0);
    
    color = lerp(color, float3(0.0, 0.0, 0.5), saturate(t * 4.0));
    color = lerp(color, float3(0.0, 1.0, 1.0), saturate(t * 4.0 - 1.0));
    color = lerp(color, float3(0.0, 1.0, 0.0), saturate(t * 4.0 - 2.0));
    color = lerp(color, float3(1.0, 0.0, 0.0), saturate(t * 4.0 - 3.0));
    
    float alpha = saturate(t * 4.0);
    return float4(color, alpha);
}

void WriteHeatmap(uint2 tileCoord, uint threadFlatIndex)
{
    if (threadFlatIndex != 0)
        return;

    float heat = saturate((float) hitCount / 16.0);
    float4 color = HeatColor(heat);

    for (uint y = 0; y < TILE_SIZE; ++y)
    {
        for (uint x = 0; x < TILE_SIZE; ++x)
        {
            uint2 pixel = tileCoord * TILE_SIZE + uint2(x, y);
            if (all(pixel < ScreenSize))
                DebugHitMap[pixel] = color;
        }
    }
}

#endif // LIGHT_CULLING_COMMON_HLSL

