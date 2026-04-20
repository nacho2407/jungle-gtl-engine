#include "Common/ForwardLightData.hlsli"
#include "Common/ConstantBuffers.hlsli"

cbuffer TileCulling : register(b2)
{
    uint2 ScreenSize;
    uint Enable25DCulling;
    float NearZ;
    float FarZ;
    uint NumLights;
    float Padding[2];
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

Texture2D<float> gDepthTexture : register(t0);
RWStructuredBuffer<uint> RWTileLightIndices : register(u0);
RWStructuredBuffer<uint2> RWTileLightGrid : register(u1);
RWStructuredBuffer<uint> RWGlobalLightCounter : register(u2);

groupshared uint tileDepthMask; // 각 타일 별 오브젝트의 Depth Mask
groupshared uint groupMinZ; // 각 타일별 minZ
groupshared uint groupMaxZ; // 각 타일별 maxZ
groupshared uint hitCount; // 각 타일별 Light 교차 수
groupshared Frustum frustum;
groupshared uint localIndices[256];

#define NUM_SLICES  32 

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
            // 비선형 깊이를 선형으로 변환 (Reverse-Z)
            float linearZ = NearZ * FarZ / (NearZ + depthSample * (FarZ - NearZ));

            // 타일 안의 min, max depth 갱신하기
            InterlockedMin(groupMinZ, asuint(linearZ));
            InterlockedMax(groupMaxZ, asuint(linearZ));

            if (Enable25DCulling != 0)
            {
                // 로그 기반 정규화: 카메라 근처 해상도 향상
                float logNear = log(NearZ);
                float logFar  = log(FarZ);
                float depthNormalized = saturate((log(linearZ) - logNear) / (logFar - logNear));

                int sliceIndex = (int) floor(depthNormalized * NUM_SLICES);
                sliceIndex = clamp(sliceIndex, 0, NUM_SLICES - 1);
                uint sliceBit = 1u << sliceIndex;
                InterlockedOr(tileDepthMask, sliceBit);
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
            // 광원 구체의 Z 범위를 로그 기반으로 정규화 (픽셀 깊이 마스크와 동일한 공간)
            float logNear = log(NearZ);
            float logFar  = log(FarZ);
            float clampedMin = max(lightMinZ, NearZ);
            float clampedMax = min(lightMaxZ, FarZ);
            float normMin = saturate((log(clampedMin) - logNear) / (logFar - logNear));
            float normMax = saturate((log(clampedMax) - logNear) / (logFar - logNear));

            int sphereSliceMin = (int) floor(normMin * NUM_SLICES);
            int sphereSliceMax = (int) floor(normMax * NUM_SLICES);
            
            sphereSliceMin = clamp(sphereSliceMin, 0, NUM_SLICES - 1);
            sphereSliceMax = clamp(sphereSliceMax, 0, NUM_SLICES - 1);
            
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
            if(slot < 256)
            {
                localIndices[slot] = i;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (threadID.x == 0 && threadID.y == 0)
    {
        uint offset;
        uint actualCount = min(hitCount, 256);
        
        InterlockedAdd(RWGlobalLightCounter[0], actualCount, offset);
        RWTileLightGrid[flatTileIndex] = uint2(offset, actualCount);
        
        for (uint i = 0; i < actualCount; i++)
        {
            RWTileLightIndices[offset + i] = localIndices[i];
        }
    }
}