#include "Common/ForwardLightData.hlsli"
#include "Common/ConstantBuffers.hlsli"

cbuffer TileCulling : register(b2)
{
    uint2 ScreenSize;
    uint Enable25DCulling;
    float NearZ;
    float FarZ;
    uint NumLights;
    float2 _tilePad;
};

cbuffer VisualizeCulling : register(b3)
{
    uint Visualize25DCulling;
    uint CursorTileX;
    uint CursorTileY;
    float _visPad;
    float4x4 InvView;
};

struct Frustum
{
    float4 planes[4];
};

struct Sphere
{
    float3 position;
    float radius;
};

#define NUM_SLICES  32
#define LOCAL_LIGHT_MAx 256

Texture2D<float> gDepthTexture : register(t0);
RWStructuredBuffer<uint> RWTileLightIndices : register(u0);
RWStructuredBuffer<uint2> RWTileLightGrid : register(u1);
RWStructuredBuffer<uint> RWGlobalLightCounter : register(u2);
RWStructuredBuffer<float4> RWVisBuffer : register(u3);

groupshared uint tileDepthMask; // 각 타일 별 오브젝트의 Depth Mask
groupshared uint groupMinZ; // 각 타일별 minZ
groupshared uint groupMaxZ; // 각 타일별 maxZ
groupshared uint hitCount; // 각 타일별 Light 교차 수
groupshared Frustum frustum;
groupshared uint localIndices[LOCAL_LIGHT_MAx];

// ── Depth 유틸리티 함수 ─────────────────────────────────────────
// Reverse-Z 비선형 깊이 → 선형 깊이
float LinearizeDepth(float rawDepth)
{
    return NearZ * FarZ / (NearZ + rawDepth * (FarZ - NearZ));
}

// 선형 깊이 → [0,1] 로그 정규화 값
float LogDepthNormalize(float linearZ)
{
    float logNear = log(NearZ);
    float logFar  = log(FarZ);
    return saturate((log(linearZ) - logNear) / (logFar - logNear));
}

// 슬라이스 경계 인덱스 [0..NUM_SLICES] → 선형 깊이
float SliceBoundaryDepth(uint sliceIndex)
{
    float logNear = log(NearZ);
    float logFar  = log(FarZ);
    float t = (float)sliceIndex / (float)NUM_SLICES;
    return exp(logNear + t * (logFar - logNear));
}

// ── 기하 유틸리티 ───────────────────────────────────────────────
float4 ComputePlane(float3 a, float3 b)
{
    float3 origin = float3(0, 0, 0); // 카메라 원점
    float3 normal = normalize(cross(a, b));
    float d = dot(normal, origin);
    return float4(normal, -d);
}

Frustum MakeFrustum(uint2 tileCoord)
{
    Frustum frustum;

    // 타일의 네개의 꼭짓점으로 절두체 만들기
    float2 corners[4];
    corners[0] = tileCoord * TILE_SIZE; // 좌상단
    corners[1] = tileCoord * TILE_SIZE + float2(TILE_SIZE, 0); // 우상단
    corners[2] = tileCoord * TILE_SIZE + float2(0, TILE_SIZE); // 좌하단
    corners[3] = tileCoord * TILE_SIZE + float2(TILE_SIZE, TILE_SIZE); // 우하단

    float3 viewCorners[4];
    for (uint i = 0; i < 4; i++)
    {
        float2 uv = corners[i] / ScreenSize;
        uv.y = 1.0f - uv.y;
        float2 ndc = uv * 2.0f - 1.0f;

        viewCorners[i] = float3(
        ndc.x * NearZ / Projection[0][0],
        ndc.y * NearZ / Projection[1][1],
        NearZ
        );
    }

    frustum.planes[0] = ComputePlane(viewCorners[0], viewCorners[2]); // 왼쪽 면
    frustum.planes[1] = ComputePlane(viewCorners[3], viewCorners[1]); // 오른쪽 면
    frustum.planes[2] = ComputePlane(viewCorners[1], viewCorners[0]); // 위쪽 면
    frustum.planes[3] = ComputePlane(viewCorners[2], viewCorners[3]); // 아래쪽 면

    return frustum;
}

bool SphereInsideFrustum(Sphere sphere, Frustum frustum)
{
    for (int i = 0; i < 4; i++)
    {
        if (dot(frustum.planes[i].xyz, sphere.position) + frustum.planes[i].w < -sphere.radius)
            return false;
    }
    return true;
}

// ── 2.5D Culling Visualization: 타일의 로그 슬라이스 경계를 월드 좌표로 기록 ──
void WriteSliceVisualization(uint2 tileCoord)
{
    if (Visualize25DCulling == 0) return;

    int dx = (int)tileCoord.x - (int)CursorTileX;
    int dy = (int)tileCoord.y - (int)CursorTileY;
    if (dx != 0 || dy != 0) return;

    uint baseOffset = 0; // 타일 1개만 사용

    // 타일의 4 코너 (스크린 좌표)
    float2 tileCorners[4];
    tileCorners[0] = float2(tileCoord * TILE_SIZE);                                // TL
    tileCorners[1] = float2(tileCoord * TILE_SIZE + uint2(TILE_SIZE, 0));           // TR
    tileCorners[2] = float2(tileCoord * TILE_SIZE + uint2(0, TILE_SIZE));           // BL
    tileCorners[3] = float2(tileCoord * TILE_SIZE + uint2(TILE_SIZE, TILE_SIZE));   // BR

    for (uint b = 0; b <= NUM_SLICES; b++)
    {
        float linearZ = SliceBoundaryDepth(b);

        for (uint c = 0; c < 4; c++)
        {
            float2 uv = tileCorners[c] / float2(ScreenSize);
            uv.y = 1.0f - uv.y;
            float2 ndc = uv * 2.0f - 1.0f;

            float3 viewPos = float3(
                ndc.x * linearZ / Projection[0][0],
                ndc.y * linearZ / Projection[1][1],
                linearZ
            );

            // 뷰 → 월드 변환
            float3 worldPos = mul(float4(viewPos, 1.0), InvView).xyz;

            RWVisBuffer[baseOffset + b * 4 + c] = float4(worldPos, 1.0);
        }
    }
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void mainCS(uint3 groupID : SV_GroupID, uint3 dispatchID:SV_DispatchThreadID, uint3 threadID : SV_GroupThreadID)
{
    // 현재 픽셀을 담당하는 스레드의 위치 찾기
    uint2 tileCoord = groupID.xy;
    uint2 pixel = tileCoord * TILE_SIZE + threadID.xy;
    uint2 screenSize = ScreenSize;

    if(threadID.x == 0 && threadID.y == 0)
    {
        tileDepthMask = 0;
        groupMinZ = 0x7f7fffff;
        groupMaxZ = 0x00000000;
        hitCount = 0;

        frustum = MakeFrustum(tileCoord);
    }
    GroupMemoryBarrierWithGroupSync();

    float depthSample = 1.0f;
    // 현재 화면에서 벗어나는 픽셀인지 검사
    if (all(pixel < screenSize))
    {
        // 해당하는 픽셀의 Depth값 가져오기
        depthSample = gDepthTexture[pixel];

        if(depthSample > 0.0f)
        {
            float linearZ = LinearizeDepth(depthSample);

            // 타일 안의 min, max depth 갱신하기
            InterlockedMin(groupMinZ, asuint(linearZ));
            InterlockedMax(groupMaxZ, asuint(linearZ));

            if (Enable25DCulling != 0)
            {
                int sliceIndex = (int) floor(LogDepthNormalize(linearZ) * NUM_SLICES);
                sliceIndex = clamp(sliceIndex, 0, NUM_SLICES - 1);
                InterlockedOr(tileDepthMask, 1u << sliceIndex);
            }
        }
    }
    // 모든 Depth값이 기록 되고 다음 로직으로
    GroupMemoryBarrierWithGroupSync();

    uint numTilesX = (ScreenSize.x + TILE_SIZE - 1) / TILE_SIZE;
    uint flatTileIndex = tileCoord.y * numTilesX + tileCoord.x;

    float tileMinZ = asfloat(groupMinZ);
    float tileMaxZ = asfloat(groupMaxZ);

    // 스레드마다 나눠서 광원이 타일 안에 있는지 검사
    uint threadIndex = threadID.y * TILE_SIZE + threadID.x;
    for (uint i = threadIndex; i < NumLights; i += TILE_SIZE * TILE_SIZE)
    {
        FLightInfo light = AllLights[i];
        Sphere s;
        s.position = mul(float4(light.Position, 1), View).xyz;
        s.radius = light.AttenuationRadius;

        // 광원의 bounding sphere의 Min, Max
        float lightMinZ = s.position.z - s.radius;
        float lightMaxZ = s.position.z + s.radius;

        // 절두체 안에 들어오지 않았으면 컬링
        if (!SphereInsideFrustum(s, frustum))
            continue;

        // (1차 검사) 광원이 오브젝트가 존재하는 범위 안에 없으면 컬링
        if (tileMinZ > tileMaxZ || lightMaxZ < tileMinZ || lightMinZ > tileMaxZ)
            continue;

        // (2차 검사) 실제로 광원의 범위와 물체가 겹치는지 검사 후 컬링
        bool depthOverlap = true;
        if(Enable25DCulling != 0)
        {
            float normMin = LogDepthNormalize(max(lightMinZ, NearZ));
            float normMax = LogDepthNormalize(min(lightMaxZ, FarZ));

            int sphereSliceMin = clamp((int) floor(normMin * NUM_SLICES), 0, NUM_SLICES - 1);
            int sphereSliceMax = clamp((int) floor(normMax * NUM_SLICES), 0, NUM_SLICES - 1);

            uint sphereMask = 0;
            for (int j = sphereSliceMin; j <= sphereSliceMax; ++j)
            {
                sphereMask |= (1u << j);
            }

            depthOverlap = (sphereMask & tileDepthMask) != 0;
        }

        if (depthOverlap)
        {
            uint slot;
            InterlockedAdd(hitCount, 1, slot);
            if (slot < LOCAL_LIGHT_MAx)
            {
                localIndices[slot] = i;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (threadID.x == 0 && threadID.y == 0)
    {
        uint offset;
        uint actualCount = min(hitCount, LOCAL_LIGHT_MAx);

        InterlockedAdd(RWGlobalLightCounter[0], actualCount, offset);
        RWTileLightGrid[flatTileIndex] = uint2(offset, actualCount);

        for (uint i = 0; i < actualCount; i++)
        {
            RWTileLightIndices[offset + i] = localIndices[i];
        }

        // ── 2.5D Culling Visualization ──
        WriteSliceVisualization(tileCoord);
    }
}
