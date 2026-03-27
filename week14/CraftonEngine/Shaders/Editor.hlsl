/* Constant Buffers */
#include "Common.hlsl"

/* Editor */
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD0;
};

PSInput VS(VSInput input)
{
    PSInput output;
    
    float3 worldPos = input.position.xyz;
    output.worldPos = worldPos;

    float4 viewPos = mul(float4(worldPos, 1.0f), View);
    output.position = mul(viewPos, Projection);

    output.color = input.color;

    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    return input.color;
}