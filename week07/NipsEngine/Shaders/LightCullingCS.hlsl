#include "Common.hlsl"
#include "Lighting.hlsl"

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 64

Texture2D<float> DepthTexture : register(t0);
RWStructuredBuffer<uint> CulledIndexBuffer : register(u0);
RWStructuredBuffer<uint2> TileBuffer : register(u1);

groupshared uint MinDepth;
groupshared uint MaxDepth;
groupshared uint TileLightCount;
groupshared uint TileLightIndices[MAX_LIGHTS_PER_TILE];

float4 ComputePlane(float3 p0, float3 p1, float3 p2)
{
    float4 plane;
    float3 v0 = p1 - p0;
    float3 v1 = p2 - p0;
    plane.xyz = normalize(cross(v0, v1));
    plane.w = dot(plane.xyz, p0);
    return plane;
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint threadIndex : SV_GroupIndex)
{
    if (threadIndex == 0)
    {
        MinDepth = 0xFFFFFFFF;
        MaxDepth = 0;
        TileLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // Depth 읽기 (Culling 확인용이지만 일단 무시)
    uint2 groupCoords = groupID.xy * TILE_SIZE + groupThreadID.xy;
    float depth = DepthTexture.Load(uint3(groupCoords, 0));

    if (depth > 0.0f && depth < 1.0f)
    {
        uint depthInt = asuint(depth);
        InterlockedMin(MinDepth, depthInt);
        InterlockedMax(MaxDepth, depthInt);
    }
    GroupMemoryBarrierWithGroupSync();
    
    float minDepth = asfloat(MinDepth);
    float maxDepth = asfloat(MaxDepth);
    
    float2 tileMin = (float2(groupID.xy) * TILE_SIZE) / ViewportSize * 2.0f - 1.0f;
    float2 tileMax = (float2(groupID.xy + 1) * TILE_SIZE) / ViewportSize * 2.0f - 1.0f;
    tileMin.y *= -1.0f;
    tileMax.y *= -1.0f;
    
    float tileMinViewZ = LinearizeDepth(minDepth);
    float tileMaxViewZ = LinearizeDepth(maxDepth);
    
    float tileNear = min(tileMinViewZ, tileMaxViewZ);
    float tileFar = max(tileMinViewZ, tileMaxViewZ);

    uint totalLightCount, stride;
    Lights.GetDimensions(totalLightCount, stride);
    
    for (uint i = threadIndex; i < totalLightCount; i += TILE_SIZE * TILE_SIZE)
    {
        FLightInfo light = Lights[i];
        if (light.Radius <= 0.0f)
            continue;
        
        float3 lightPosView = mul(View, float4(light.Position, 1.0f)).xyz;
        float radius = light.Radius;
        
        float lightZ = lightPosView.z;
        bool isInside = true;

        if (lightZ + radius < tileNear || lightZ - radius > tileFar)
            isInside = false;
        
        if (isInside)
        {
            uint index;
            InterlockedAdd(TileLightCount, 1, index);
            if (index < MAX_LIGHTS_PER_TILE)
                TileLightIndices[index] = i;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (threadIndex == 0)
    {
        uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint flatTileIndex = groupID.y * numTilesX + groupID.x;
        uint storageOffset = flatTileIndex * MAX_LIGHTS_PER_TILE;

        for (uint j = 0; j < TileLightCount; j++)
        {
            CulledIndexBuffer[storageOffset + j] = TileLightIndices[j];
        }
        
        TileBuffer[flatTileIndex] = uint2(storageOffset, TileLightCount);
    }
}