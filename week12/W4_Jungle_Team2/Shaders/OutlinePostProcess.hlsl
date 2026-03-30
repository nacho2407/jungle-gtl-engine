#include "Common.hlsl"

Texture2D<uint2> StencilTexture : register(t7);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VS(uint vertexId : SV_VertexID)
{
    VSOutput output;

    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    float2 uvs[3] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };

    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.uv = uvs[vertexId];
    return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
    const int2 viewportSize = int2(max(OutlineViewportSize, float2(1.0f, 1.0f)));
    const int2 pixelCoord = int2(input.position.xy);
    //  Viewport 안쪽으로 clamp
    const int2 clampedCoord = clamp(pixelCoord, int2(0, 0), viewportSize - 1);

    const uint2 centerSample = StencilTexture.Load(int3(clampedCoord, 0));
    //  실제로는 depth와 stencil이 섞여서 들어옴
    const uint centerStencil = max(centerSample.x, centerSample.y);
    if (centerStencil != 0u)
    {
        discard;
    }

    const int radius = max((int)round(OutlineThicknessPixels), 1);
    const int2 neighborOffsets[8] =
    {
        int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1),
        int2(-1, -1), int2(-1, 1), int2(1, -1), int2(1, 1)
    };

    for (int r = 1; r <= radius; ++r)
    {
        [unroll]
        for (int i = 0; i < 8; ++i)
        {
            const int2 sampleCoord = clamp(clampedCoord + neighborOffsets[i] * r, int2(0, 0), viewportSize - 1);
            const uint2 neighborSample = StencilTexture.Load(int3(sampleCoord, 0));
            if (max(neighborSample.x, neighborSample.y) != 0u)
            {
                return OutlineColor;
            }
        }
    }

    discard;
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
