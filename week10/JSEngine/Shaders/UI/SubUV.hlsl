#include "../Common/Common.hlsli"

Texture2D SubUVAtlas : register(t0);
SamplerState SubUVSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = mul(mul(float4(input.position, 1.0f), View), Projection);
    output.texCoord = input.texCoord;
    output.color = input.color;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 col = SubUVAtlas.Sample(SubUVSampler, input.texCoord);
    if (bIsWireframe < 0.5f)
    {
        if (col.a < 0.1f || (col.r < 0.1f && col.g < 0.1f && col.b < 0.1f))
        {
            discard;
        }
        return col * input.color;
    }

    return lerp(col, float4(WireframeRGB, 1.0f), bIsWireframe);
}
