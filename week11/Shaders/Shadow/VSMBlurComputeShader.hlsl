// VSMBlurCS.hlsl
// Two-pass separable Gaussian blur for VSM Shadow Atlas
// HORIZONTAL_PASS=1: 수평, 미정의: 수직

#define KERNEL_RADIUS 4
#define GROUP_SIZE_X  8
#define GROUP_SIZE_Y  8

static const float GaussWeights[9] =
{
    0.0162, 0.0540, 0.1216, 0.1945, 0.2270,
    0.1945, 0.1216, 0.0540, 0.0162
};

Texture2D<float2> SrcTexture : register(t0);
RWTexture2D<float2> DstTexture : register(u0);

cbuffer BlurCB : register(b11)
{
    // 이 Dispatch가 처리할 타일의 픽셀 공간 rect
    // (AtlasOffsetX, AtlasOffsetY): 타일 좌상단 픽셀 좌표
    // (TileWidth, TileHeight):      타일 크기 (픽셀)
    uint2 AtlasOffset; // 타일 좌상단 (픽셀)
    uint2 TileSize; // 타일 크기   (픽셀)
};

#if HORIZONTAL_PASS
groupshared float2 Cache[GROUP_SIZE_X + 2 * KERNEL_RADIUS][GROUP_SIZE_Y];
#else
groupshared float2 Cache[GROUP_SIZE_X][GROUP_SIZE_Y + 2 * KERNEL_RADIUS];
#endif

// 타일 rect 안으로 픽셀 좌표를 clamp하는 헬퍼
int2 ClampToTile(int2 Pixel)
{
    int2 TileMin = int2(AtlasOffset);
    int2 TileMax = int2(AtlasOffset + TileSize) - 1;
    return clamp(Pixel, TileMin, TileMax);
}

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(
    uint3 groupID : SV_GroupID,
    uint3 groupThreadID : SV_GroupThreadID)
{
    // Dispatch는 타일 단위로 호출되므로
    // 실제 atlas 픽셀 좌표 = 타일 오프셋 + group 내 좌표
    int2 texel = int2(AtlasOffset)
               + int2(groupID.xy) * int2(GROUP_SIZE_X, GROUP_SIZE_Y)
               + int2(groupThreadID.xy);

#if HORIZONTAL_PASS
    int cacheX = (int)groupThreadID.x + KERNEL_RADIUS;
    int cacheY = (int)groupThreadID.y;

    // 중앙
    Cache[cacheX][cacheY] = SrcTexture[ClampToTile(texel)];

    // 왼쪽 halo — 타일 경계 clamp 적용
    if ((int)groupThreadID.x < KERNEL_RADIUS)
    {
        int2 left = ClampToTile(int2(texel.x - KERNEL_RADIUS, texel.y));
        Cache[cacheX - KERNEL_RADIUS][cacheY] = SrcTexture[left];
    }
    // 오른쪽 halo
    if ((int)groupThreadID.x >= GROUP_SIZE_X - KERNEL_RADIUS)
    {
        int2 right = ClampToTile(int2(texel.x + KERNEL_RADIUS, texel.y));
        Cache[cacheX + KERNEL_RADIUS][cacheY] = SrcTexture[right];
    }

    GroupMemoryBarrierWithGroupSync();

    // 타일 범위 밖 thread는 쓰기 생략
    int2 TileMax = int2(AtlasOffset + TileSize);
    if (texel.x >= TileMax.x || texel.y >= TileMax.y)
        return;

    float2 result = float2(0.0, 0.0);
    [unroll]
    for (int k = -KERNEL_RADIUS; k <= KERNEL_RADIUS; ++k)
        result += GaussWeights[k + KERNEL_RADIUS] * Cache[cacheX + k][cacheY];

    DstTexture[texel] = result;

#else
    int cacheX = (int) groupThreadID.x;
    int cacheY = (int) groupThreadID.y + KERNEL_RADIUS;

    Cache[cacheX][cacheY] = SrcTexture[ClampToTile(texel)];

    if ((int) groupThreadID.y < KERNEL_RADIUS)
    {
        int2 top = ClampToTile(int2(texel.x, texel.y - KERNEL_RADIUS));
        Cache[cacheX][cacheY - KERNEL_RADIUS] = SrcTexture[top];
    }
    if ((int) groupThreadID.y >= GROUP_SIZE_Y - KERNEL_RADIUS)
    {
        int2 bot = ClampToTile(int2(texel.x, texel.y + KERNEL_RADIUS));
        Cache[cacheX][cacheY + KERNEL_RADIUS] = SrcTexture[bot];
    }

    GroupMemoryBarrierWithGroupSync();

    int2 TileMax = int2(AtlasOffset + TileSize);
    if (texel.x >= TileMax.x || texel.y >= TileMax.y)
        return;

    float2 result = float2(0.0, 0.0);
    [unroll]
    for (int k = -KERNEL_RADIUS; k <= KERNEL_RADIUS; ++k)
        result += GaussWeights[k + KERNEL_RADIUS] * Cache[cacheX][cacheY + k];

    DstTexture[texel] = result;
#endif
}