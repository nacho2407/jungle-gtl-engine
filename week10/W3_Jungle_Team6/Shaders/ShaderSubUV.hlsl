#include "Common.hlsl"

Texture2D    SubUVAtlas   : register(t0);
SamplerState SubUVSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = mul(mul(float4(input.position, 1.0f), View), Projection);
    output.texCoord = input.texCoord;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 col = SubUVAtlas.Sample(SubUVSampler, input.texCoord);
    // R채널이 아닌 알파 채널 기준으로 외곽선 제거
    // → 내부에 검은 픽셀이 있는 Explosion 등 컬러 스프라이트에서도 올바르게 동작
    if (col.a < 0.05f) discard;
    return col;
}
