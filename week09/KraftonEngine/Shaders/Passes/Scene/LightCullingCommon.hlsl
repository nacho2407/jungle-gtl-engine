
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

//FLocalLightInfo는 ConstnatBuffer에 정의

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
groupshared uint tileDepthMask; // 타일 내 오브젝트 Depth 비트마스크
groupshared uint groupMinZ;     // 타일 내 최솟값 (uint reinterpret)
groupshared uint groupMaxZ;     // 타일 내 최댓값 (uint reinterpret)
groupshared uint hitCount;      // 타일 내 조명 교차 수 (히트맵용)

// ============================================================
// Resources
// ============================================================
// SRV (읽기 전용)
StructuredBuffer<FLocalLightInfo> g_LightBuffer : register(t6);
Texture2D<float>              SceneDepth    : register(t1);

// UAV (읽기 / 쓰기)
RWStructuredBuffer<uint>  PerTilePointLightIndexMaskOut : register(u1); // 타일별 조명 비트마스크
RWStructuredBuffer<uint>  CulledPointLightIndexMaskOUT  : register(u2); // 전체 OR 누적 (Shadow Map용)
RWTexture2D<float4>       DebugHitMap                   : register(u3); // 히트맵 디버그 텍스처

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
    // Z 범위 검사 (View Space)
    if (sphere.position.z + sphere.radius < zNear ||
        sphere.position.z - sphere.radius > zFar)
        return false;

    // 4개 Side Plane 검사
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

// 조명(구)의 깊이 구간이 타일 오브젝트 깊이 마스크와 겹치는지 판정
bool ShouldLightAffectTile(float3 lightVSPos, float radius, float minZ, float maxZ, uint depthMask)
{
    if (depthMask == 0)
        return false;

    float s_minDepth = lightVSPos.z - radius;
    float s_maxDepth = lightVSPos.z + radius;

    // 타일 깊이 범위 밖이면 컬링
    if (s_maxDepth < minZ || s_minDepth > maxZ)
        return false;

    float rangeZ = max(1e-5, maxZ - minZ);
    float normMin = saturate((s_minDepth - minZ) / rangeZ);
    float normMax = saturate((s_maxDepth - minZ) / rangeZ);

    // 구가 여러 슬라이스에 걸칠 수 있으므로 floor/ceil로 바운드
    int sliceMin = clamp((int) floor(normMin * NUM_SLICES), 0, NUM_SLICES - 1);
    int sliceMax = clamp((int) ceil (normMax * NUM_SLICES), 0, NUM_SLICES - 1);

    uint sphereMask = 0;
    [unroll]
    for (int i = sliceMin; i <= sliceMax; ++i)
        sphereMask |= (1u << i);

    return (sphereMask & depthMask) != 0;
}

// ============================================================
// CullLight - Frustum + Depth 판정 후 비트마스크에 기록
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

    // 타일별 마스크에 기록
    InterlockedOr(MaskBuffer[flatTileIndex * SHADER_ENTITY_TILE_BUCKET_COUNT + bucketIdx], 1u << bitIdx);
    // 전체 OR 누적 마스크에 기록 (Shadow Map 최적화용)
    InterlockedOr(CulledMaskBuffer[bucketIdx], 1u << bitIdx);

    InterlockedAdd(hitCount, 1);
}

// ============================================================
// Heatmap 출력 (threadFlatIndex == 0 인 스레드만 기록)
// ============================================================
float4 HeatColor(float t)
{
    // 0.0 → 검정, 0.25 → 어두운 파랑, 0.5 → 시안, 0.75 → 연두, 1.0 → 빨강
    float3 color = float3(0, 0, 0);
    
    color = lerp(color, float3(0.0, 0.0, 0.5), saturate(t * 4.0)); // 검정 → 어두운 파랑
    color = lerp(color, float3(0.0, 1.0, 1.0), saturate(t * 4.0 - 1.0)); // 어두운 파랑 → 시안
    color = lerp(color, float3(0.0, 1.0, 0.0), saturate(t * 4.0 - 2.0)); // 시안 → 연두
    color = lerp(color, float3(1.0, 0.0, 0.0), saturate(t * 4.0 - 3.0)); // 연두 → 빨강
    
    float alpha = saturate(t * 4.0); // 빛 없으면 alpha=0 → scene color 그대로
    return float4(color, alpha);
}

void WriteHeatmap(uint2 tileCoord, uint threadFlatIndex)
{
    if (threadFlatIndex != 0)
        return;

    float heat = saturate((float) hitCount / 16.0); // 16개면 최대 (TILE_SIZE=4 기준 조정)
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
