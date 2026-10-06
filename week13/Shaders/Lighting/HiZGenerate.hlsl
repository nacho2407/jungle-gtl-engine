// Hi-Z Mip Chain Generator
// CSCopyDepth: depth buffer → Hi-Z mip 0
// CSDownsample: Hi-Z mip N-1 → Hi-Z mip N (min downsample, Reversed-Z: closer = larger)

cbuffer HiZParams : register(b0)
{
    uint2 SrcDimensions;
    uint2 _pad;
};

Texture2D<float> SrcTexture : register(t0);
RWTexture2D<float> DstTexture : register(u0);

[numthreads(8, 8, 1)]
void CSCopyDepth(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= SrcDimensions.x || DTid.y >= SrcDimensions.y)
        return;

    DstTexture[DTid.xy] = SrcTexture[DTid.xy];
}

[numthreads(8, 8, 1)]
void CSDownsample(uint3 DTid : SV_DispatchThreadID)
{
    uint2 dstSize = max(SrcDimensions / 2, uint2(1, 1));

    if (DTid.x >= dstSize.x || DTid.y >= dstSize.y)
        return;

    uint2 srcCoord = DTid.xy * 2;
    uint2 srcMax   = SrcDimensions - uint2(1, 1);

    float d00 = SrcTexture[srcCoord];
    float d10 = SrcTexture[min(srcCoord + uint2(1, 0), srcMax)];
    float d01 = SrcTexture[min(srcCoord + uint2(0, 1), srcMax)];
    float d11 = SrcTexture[min(srcCoord + uint2(1, 1), srcMax)];

    // Reversed-Z: keep the farthest (smallest) depth for conservative occlusion
    DstTexture[DTid.xy] = min(min(d00, d10), min(d01, d11));
}
