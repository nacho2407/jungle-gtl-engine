#ifndef LIGHT_CULLING_H
#define LIGHT_CULLING_H

#include "../Common/Common.hlsli"
#include "../Common/Lighting.hlsli"

struct FVisibleLightList
{
    uint Offset;
    uint Count;
};

FVisibleLightList GetVisibleLightList(float4 ClipPos)
{
    FVisibleLightList Result;
    Result.Offset = 0;
    Result.Count = LightCount;

#if CULLING_MODEL_CLUSTERED
    uint2 TileCoord = uint2(ClipPos.xy) / TILE_SIZE;
    uint NumTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint NumTilesY = (uint(ViewportSize.y) + TILE_SIZE - 1) / TILE_SIZE;
    float Z = (IsOrthographic > 0.5f)
        ? NearZ + ClipPos.z * (FarZ - NearZ)
        : abs(Projection[3][2] / (ClipPos.z - Projection[0][2]));
    uint SliceIndex = clamp(uint(log(Z / NearZ) / log(FarZ / NearZ) * NUM_SLICE), 0, NUM_SLICE - 1);
    uint2 ClusterData = TileBuffer[(SliceIndex * NumTilesY + TileCoord.y) * NumTilesX + TileCoord.x];
    Result.Offset = ClusterData.x;
    Result.Count = ClusterData.y;
#elif CULLING_MODEL_TILED
    uint2 TileCoord = uint2(ClipPos.xy) / TILE_SIZE;
    uint NumTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 TileData = TileBuffer[TileCoord.y * NumTilesX + TileCoord.x];
    Result.Offset = TileData.x;
    Result.Count = TileData.y;
#endif

    return Result;
}

uint GetVisibleLightIndex(FVisibleLightList VisibleLights, uint LightOrdinal)
{
#if CULLING_MODEL_CLUSTERED || CULLING_MODEL_TILED
    return CulledIndexBuffer[VisibleLights.Offset + LightOrdinal];
#else
    return LightOrdinal;
#endif
}

#endif
