/* Constant Buffers */
#include "Common.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 uv       : TEXTCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

/* Outline — normal-based expansion for StaticMesh */
PSInput VS(VSInput input)
{
    PSInput output;

    float3 offset = normalize(input.normal) * (OutlineOffset * OutlineInvScale);
    float3 expandedPos = input.position + offset;

    output.position = ApplyMVP(expandedPos);
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    return OutlineColor;
}
