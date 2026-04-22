#include "LightCullingCommon.hlsl"

// ============================================================
// Compute Shader Entry Point
// ============================================================
[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CS_LightCulling(
    uint3 groupID    : SV_GroupID,
    uint3 dispatchID : SV_DispatchThreadID,
    uint3 threadID   : SV_GroupThreadID,
    uint  groupIndex : SV_GroupIndex)       // 타일 내 1D 인덱스 (0 ~ TILE_SIZE*TILE_SIZE-1)
{
    uint2 tileCoord = groupID.xy;
    uint2 pixel     = tileCoord * TILE_SIZE + threadID.xy;


    // --------------------------------------------------------
    // 1. Groupshared 초기화 (첫 번째 스레드만)
    // --------------------------------------------------------
    if (groupIndex == 0)
    {
        tileDepthMask = 0;
        
#if TILE_BASED_CULLING
        groupMinZ = 0x00000000; // float max
        groupMaxZ = 0x7f7fffff; // float 0
#elif TILE_25D_CULLING_
        groupMinZ = 0x7f7fffff; // float 0
        groupMaxZ = 0x00000000; // float max
      
 #endif
        hitCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // --------------------------------------------------------
    // 2. Depth Sampling & MinZ/MaxZ 수집
    // --------------------------------------------------------
    float depthSample = 1.0;
    float linearZ     = FarZ;

    if (Enable25DCulling != 0 && all(pixel < ScreenSize))
    {
        depthSample = SceneDepth[pixel];
        if (depthSample < 1.0)
        {
            // 비선형 depth → View Space 선형 거리
// Reverse-Z 환경에서의 올바른 View Space Linear Z 변환
            linearZ = (NearZ * FarZ) / (NearZ + depthSample * (FarZ - NearZ));
            InterlockedMin(groupMinZ, asuint(linearZ));
            InterlockedMax(groupMaxZ, asuint(linearZ));
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // --------------------------------------------------------
    // 3. 타일 [minZ, maxZ] 확정 후 Depth 슬라이스 비트 기록
    // --------------------------------------------------------
    float minZ = NearZ;
    float maxZ = FarZ;

    if (Enable25DCulling != 0 && groupMaxZ > groupMinZ)
    {
        minZ = asfloat(groupMinZ);
        maxZ = asfloat(groupMaxZ);
    }

    if (Enable25DCulling != 0 && depthSample < 1.0)
    {
        float rangeZ = maxZ - minZ;
        if (rangeZ < 1e-3)
        {
            // 깊이 분포가 너무 좁으면 전체 범위로 확장 (Dithering 방지)
            minZ   = NearZ;
            maxZ   = FarZ;
            rangeZ = maxZ - minZ;
        }
        float normZ    = saturate((linearZ - minZ) / rangeZ);
        int   slice    = clamp((int) floor(normZ * NUM_SLICES), 0, NUM_SLICES - 1);
        InterlockedOr(tileDepthMask, 1u << slice);
    }
    GroupMemoryBarrierWithGroupSync();

    // --------------------------------------------------------
    // 4. Tile Frustum 계산 (View Space 8 꼭짓점 → 4 평면)
    // --------------------------------------------------------
    float2 tileMin = float2(tileCoord * TileSize);
    float2 tileMax = tileMin + float2(TileSize);
    // HLSL의 float4x4 생성자는 행(Row) 단위로 값을 채워 넣습니다.
    float4x4 InverseProjection = float4x4(
    1.0f / Projection._11, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f / Projection._22, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f / Projection._43,
    0.0f, 0.0f, 1.0f, -Projection._33 / Projection._43
);
    float3 viewCorners[8];
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        float2 uv = float2(
            (i & 1) ? tileMax.x : tileMin.x,
            (i & 2) ? tileMax.y : tileMin.y
        ) / float2(ScreenSize);

        uv.y = 1.0 - uv.y; // D3D UV → NDC Y 반전

// Reverse-Z 환경이라면 Z값을 반대로 뒤집어야 합니다.
        float4 clipNear = float4(uv * 2.0 - 1.0, 1.0, 1.0); // Near Plane
        float4 clipFar = float4(uv * 2.0 - 1.0, 0.0, 1.0); // Far Plane
        float4 viewNear = mul(clipNear, InverseProjection);
        float4 viewFar = mul(clipFar, InverseProjection);

        viewCorners[i + 0] = viewNear.xyz / viewNear.w;
        viewCorners[i + 4] = viewFar.xyz  / viewFar.w;
    }

    // corners 배치
    // [0] = (tileMin.x, tileMin.y) Near  [4] = (tileMin.x, tileMin.y) Far
    // [1] = (tileMax.x, tileMin.y) Near  [5] = (tileMax.x, tileMin.y) Far
    // [2] = (tileMin.x, tileMax.y) Near  [6] = (tileMin.x, tileMax.y) Far
    // [3] = (tileMax.x, tileMax.y) Near  [7] = (tileMax.x, tileMax.y) Far

    FFrustum frustum;
    frustum.planes[0] = ComputePlane(viewCorners[0], viewCorners[2], viewCorners[6]); // 좌
    frustum.planes[1] = ComputePlane(viewCorners[3], viewCorners[1], viewCorners[7]); // 우
    frustum.planes[2] = ComputePlane(viewCorners[1], viewCorners[0], viewCorners[5]); // 하
    frustum.planes[3] = ComputePlane(viewCorners[2], viewCorners[3], viewCorners[7]); // 상

    // --------------------------------------------------------
    // 5. Light Culling
    // --------------------------------------------------------
    uint totalThreads = TILE_SIZE * TILE_SIZE;
    uint NumTilesX    = (ScreenSize.x + TILE_SIZE - 1) / TILE_SIZE;
    uint flatTileIndex = groupID.y * NumTilesX + groupID.x;

    // Point Light
    for (uint idx = groupIndex; idx < (uint)NumLights; idx += totalThreads)
    {
        float3 lightVSPos = mul(float4(g_LightBuffer[idx].Position, 1.0), View).xyz;
        CullLight(
            idx, lightVSPos, g_LightBuffer[idx].AttenuationRadius,
            frustum, minZ, maxZ,
            flatTileIndex,
            PerTilePointLightIndexMaskOut,
            CulledPointLightIndexMaskOUT
        );
    }

    // Spot Light 등 추가 시 동일 패턴으로 확장

    // --------------------------------------------------------
    // 6. Heatmap 기록
    // --------------------------------------------------------
    GroupMemoryBarrierWithGroupSync();
    WriteHeatmap(tileCoord, groupIndex);
}
