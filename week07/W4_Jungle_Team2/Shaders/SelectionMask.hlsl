#include "Common.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = ApplyMVP(input.position);
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
