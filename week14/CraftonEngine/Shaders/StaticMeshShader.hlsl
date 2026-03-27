#include "Common.hlsl"

struct VS_INPUT
{
    float3 p : POSITION;
    float3 n : NORMAL;
    float4 c : COLOR;
    float2 t : TEXTURE;
};

struct PS_INPUT
{
    float4 p : SV_POSITION;
    float3 n : NORMAL;
    float4 c : COLOR;
    float2 t : TEXTURE;
};

Texture2D    g_txColor : register(t0);
SamplerState g_Sample  : register(s0);

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.p = mul(float4(input.p, 1.0f), WVP);
    output.n = normalize(mul(input.n, (float3x3)World));
    output.c = input.c;
    output.t = input.t;
    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    return input.c;
}
