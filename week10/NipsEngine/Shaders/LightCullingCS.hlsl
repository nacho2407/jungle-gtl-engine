#define CS_SHADER
#include "Common.hlsl"
#include "Lighting.hlsl"

RWStructuredBuffer<uint>  CulledIndexBuffer : register(u0);
RWStructuredBuffer<uint2> TileBuffer        : register(u1);

#define MAX_LIGHTS_PER_TILE 64

Texture2D<float> DepthTexture : register(t0);

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

    uint2 groupCoords = groupID.xy * TILE_SIZE + groupThreadID.xy;
    float depth = DepthTexture.Load(uint3(groupCoords, 0));

    if (depth > 0.0f && depth < 1.0f)
    {
        uint depthInt = asuint(depth);
        InterlockedMin(MinDepth, depthInt);
        InterlockedMax(MaxDepth, depthInt);
    }
    GroupMemoryBarrierWithGroupSync();
    
    float tileNear, tileFar;
    if (MinDepth == 0xFFFFFFFF)
    {
        tileNear = NearZ;
        tileFar = FarZ;
    }
    else
    {
        tileNear = LinearizeDepth(asfloat(MinDepth));
        tileFar = LinearizeDepth(asfloat(MaxDepth));
    }
    
    float ndcL = (float(groupID.x)     * TILE_SIZE) / ViewportSize.x * 2.0f - 1.0f;
    float ndcR = (float(groupID.x + 1) * TILE_SIZE) / ViewportSize.x * 2.0f - 1.0f;
    float ndcB = 1.0f - (float(groupID.y + 1) * TILE_SIZE) / ViewportSize.y * 2.0f;
    float ndcT = 1.0f - (float(groupID.y)     * TILE_SIZE) / ViewportSize.y * 2.0f;

    float P00 = Projection[1][0];
    float P11 = Projection[2][1];

    float3 planeL = normalize(float3(-ndcL,  P00, 0.0f));
    float3 planeR = normalize(float3( ndcR, -P00, 0.0f));
    float3 planeB = normalize(float3(-ndcB, 0.0f,  P11));
    float3 planeT = normalize(float3( ndcT, 0.0f, -P11));

    for (uint i = threadIndex; i < LightCount; i += TILE_SIZE * TILE_SIZE)
    {
        FLightInfo light = Lights[i];
        if (light.Radius <= 0.0f)
            continue;

        float3 lightPosView = mul(float4(light.Position, 1.0f), View).xyz;
        float radius = light.Radius;

        if (lightPosView.x + radius < tileNear || lightPosView.x - radius > tileFar)
            continue;

        if (dot(planeL, lightPosView) < -radius) continue;
        if (dot(planeR, lightPosView) < -radius) continue;
        if (dot(planeB, lightPosView) < -radius) continue;
        if (dot(planeT, lightPosView) < -radius) continue;
        
        uint index;
        InterlockedAdd(TileLightCount, 1, index);
        if (index < MAX_LIGHTS_PER_TILE)
            TileLightIndices[index] = i;
    }
    GroupMemoryBarrierWithGroupSync();

    if (threadIndex == 0)
    {
        uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint flatTileIndex = groupID.y * numTilesX + groupID.x;
        uint storageOffset = flatTileIndex * MAX_LIGHTS_PER_TILE;

        uint clampedCount = min(TileLightCount, MAX_LIGHTS_PER_TILE);
        for (uint j = 0; j < clampedCount; j++)
        {
            CulledIndexBuffer[storageOffset + j] = TileLightIndices[j];
        }
        
        TileBuffer[flatTileIndex] = uint2(storageOffset, min(TileLightCount, MAX_LIGHTS_PER_TILE));
    }
}