cbuffer HiZConstants : register(b0)
{
    uint SrcWidth;
    uint SrcHeight;
    uint DstWidth;
    uint DstHeight;
};

Texture2D<float>   SrcMip : register(t0);
RWTexture2D<float> DstMip : register(u0);

[numthreads(8, 8, 1)]
void HiZCopyMip0CS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= DstWidth || DTid.y >= DstHeight) return;

    DstMip[DTid.xy] = SrcMip[DTid.xy];
}

[numthreads(8, 8, 1)]
void HiZDownsampleCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= DstWidth || DTid.y >= DstHeight) return;

    // Reverse-Z keeps the farthest depth as the minimum numeric value.
    // Out-of-bounds loads return 0 in D3D11, which stays conservative for min reduction.
    uint2 Src = DTid.xy * 2;
    float d0 = SrcMip[Src];
    float d1 = SrcMip[Src + uint2(1, 0)];
    float d2 = SrcMip[Src + uint2(0, 1)];
    float d3 = SrcMip[Src + uint2(1, 1)];

    DstMip[DTid.xy] = min(min(d0, d1), min(d2, d3));
}
