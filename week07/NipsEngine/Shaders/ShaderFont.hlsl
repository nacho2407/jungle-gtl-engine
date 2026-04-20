#include "Common.hlsl"

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

struct VSInput
{
    float3 position : POSITION; // CPU Billboard — 월드 좌표
    float2 texCoord : TEXCOORD;
    float4 color    : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 color    : COLOR;
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
    float4 col = FontAtlas.Sample(FontSampler, input.texCoord) * input.color;
    if (bIsWireframe < 0.5f)
    {
        if (col.r < 0.1f)
        {
            discard; // 검은 배경 제거    
        }
        return col;
    }
    
    return float4(WireframeRGB, 1.0f);

}
