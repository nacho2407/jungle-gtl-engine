#define CS_SHADER
#include "../Common/Common.hlsli"
#include "../Common/Lighting.hlsli"

RWStructuredBuffer<uint>  CulledIndexBuffer : register(u0);
RWStructuredBuffer<uint2> TileBuffer        : register(u1);

#define MAX_LIGHTS_PER_TILE 128
#define MAX_LIGHTS_SCRATCH  512

Texture2D<float> DepthTexture : register(t0);

#if CULLING_MODEL_TILED
groupshared uint MinDepth;
groupshared uint MaxDepth;
#endif
groupshared uint TileLightCount;
groupshared uint TileLightIndices[MAX_LIGHTS_SCRATCH];
groupshared float TileImportance[MAX_LIGHTS_SCRATCH];

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint threadIndex : SV_GroupIndex)
{
#if CULLING_MODEL_CLUSTERED
    if (threadIndex == 0)
        TileLightCount = 0;
#elif CULLING_MODEL_TILED
    if (threadIndex == 0) {
        TileLightCount = 0;
        MinDepth = 0xFFFFFFFF;
        MaxDepth = 0;
    }
#endif
    GroupMemoryBarrierWithGroupSync();
    
#if CULLING_MODEL_CLUSTERED
    float sliceNear = NearZ * pow(FarZ / NearZ, float(groupID.z) / NUM_SLICE);
    float sliceFar  = NearZ * pow(FarZ / NearZ, float(groupID.z + 1) / NUM_SLICE);
#elif CULLING_MODEL_TILED
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
        tileFar  = FarZ;
    }
    else
    {
        tileNear = LinearizeDepth(asfloat(MinDepth));
        tileFar  = LinearizeDepth(asfloat(MaxDepth));
    }
#endif
    
    float ndcL = (float(groupID.x)     * TILE_SIZE) / ViewportSize.x * 2.0f - 1.0f;
    float ndcR = (float(groupID.x + 1) * TILE_SIZE) / ViewportSize.x * 2.0f - 1.0f;
    float ndcB = 1.0f - (float(groupID.y + 1) * TILE_SIZE) / ViewportSize.y * 2.0f;
    float ndcT = 1.0f - (float(groupID.y)     * TILE_SIZE) / ViewportSize.y * 2.0f;

    float P00 = Projection[1][0];
    float P11 = Projection[2][1];

    float3 planeL = normalize(float3(-ndcL, P00, 0.0f));
    float3 planeR = normalize(float3( ndcR, -P00, 0.0f));
    float3 planeB = normalize(float3(-ndcB, 0.0f,  P11));
    float3 planeT = normalize(float3( ndcT, 0.0f, -P11));
    
    float orthoL, orthoR, orthoB, orthoT;
    if (IsOrthographic > 0.5f)
    {
        float invScaleX = 1.0f / P00;
        float invScaleY = 1.0f / P11;
        
        orthoL = ndcL * invScaleX;
        orthoR = ndcR * invScaleX;
        orthoB = ndcB * invScaleY;
        orthoT = ndcT * invScaleY;
    }

    for (uint i = threadIndex; i < LightCount; i += TILE_SIZE * TILE_SIZE)
    {
        LightInfo light = Lights[i];
        if (light.Radius <= 0.0f)
            continue;

        float3 lightPosView = mul(float4(light.Position, 1.0f), View).xyz;
        float radius = light.Radius;

#if CULLING_MODEL_CLUSTERED
        if (lightPosView.x + radius < sliceNear || lightPosView.x - radius > sliceFar)
            continue;
#elif CULLING_MODEL_TILED
        if (lightPosView.x + radius < tileNear || lightPosView.x - radius > tileFar)
            continue;
#endif

        if (IsOrthographic > 0.5f)
        {
            if (lightPosView.y + radius < orthoL || lightPosView.y - radius > orthoR)
                continue;
            if (lightPosView.z + radius < orthoB || lightPosView.z - radius > orthoT)
                continue;
        }
        else
        {
            if (dot(planeL, lightPosView) < -radius)
                continue;
            if (dot(planeR, lightPosView) < -radius)
                continue;
            if (dot(planeB, lightPosView) < -radius)
                continue;
            if (dot(planeT, lightPosView) < -radius)
                continue;
        }
        
        uint index;
        InterlockedAdd(TileLightCount, 1, index);
        if (index < MAX_LIGHTS_SCRATCH)
        {
            TileLightIndices[index] = i;
            float3 delta = CameraPosition - lightPosView;
            float distSq = max(dot(delta, delta), 0.0001f);

            uint importance = 0;
            float weight = light.Intensity * light.Radius * length(light.Color);
            uint weightBits = (uint) (weight * 1000.f);
            importance |= weightBits << 8;
            
            uint distanceBits = (uint) (100000.f - distSq * 10.f);
            importance |= distanceBits << 4;
            
            TileImportance[index] = importance;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (threadIndex == 0)
    {
        uint scratchCount = min(TileLightCount, MAX_LIGHTS_SCRATCH);
        uint clampedCount = min(TileLightCount, MAX_LIGHTS_PER_TILE);
        // Sort all scratch candidates by importance so the top MAX_LIGHTS_PER_TILE are deterministic
        for (uint i = 1; i < scratchCount; i++)
        {
            float keyImp = TileImportance[i];
            uint keyIdx = TileLightIndices[i];
            int j = (int) i - 1;
        
            while (j >= 0 && TileImportance[j] < keyImp)
            {
                TileImportance[j + 1] = TileImportance[j];
                TileLightIndices[j + 1] = TileLightIndices[j];
                j--;
            }
        
            TileImportance[j + 1] = keyImp;
            TileLightIndices[j + 1] = keyIdx;
        }
        
#if CULLING_MODEL_CLUSTERED
        uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint numTilesY = (uint(ViewportSize.y) + TILE_SIZE - 1) / TILE_SIZE;
        uint flatClusterIndex = (groupID.z * numTilesY + groupID.y) * numTilesX + groupID.x;
        uint storageOffset = flatClusterIndex * MAX_LIGHTS_PER_TILE;

        for (uint j = 0; j < clampedCount; j++)
        {
            CulledIndexBuffer[storageOffset + j] = TileLightIndices[j];
        }
        
        TileBuffer[flatClusterIndex] = uint2(storageOffset, min(TileLightCount, MAX_LIGHTS_PER_TILE));
#elif CULLING_MODEL_TILED
        uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint flatTileIndex = groupID.y * numTilesX + groupID.x;
        uint storageOffset = flatTileIndex * MAX_LIGHTS_PER_TILE;

        for (uint j = 0; j < clampedCount; j++)
        {
            CulledIndexBuffer[storageOffset + j] = TileLightIndices[j];
        }
        
        TileBuffer[flatTileIndex] = uint2(storageOffset, min(TileLightCount, MAX_LIGHTS_PER_TILE));
#endif
    }
}