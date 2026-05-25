#include "../Common/Common.hlsli"

struct VSInput
{
    float3 position : POSITION0;
    float2 texCoord : TEXCOORD;
    float3 center : POSITION1;
    float3 axisX : TEXCOORD1;
    float3 axisY : TEXCOORD2;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VS(VSInput input)
{
    PSInput output;
    float3 worldPosition = input.center + input.axisX * input.position.x + input.axisY * input.position.y;
    output.position = mul(mul(float4(worldPosition, 1.0f), View), Projection);
    output.color = input.color;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    if (input.color.a <= 0.001f)
    {
        discard;
    }

    if (bIsWireframe > 0.5f)
    {
        return float4(WireframeRGB, input.color.a);
    }

    return input.color;
}
