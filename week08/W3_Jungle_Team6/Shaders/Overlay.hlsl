#include "Common.hlsl"

/* Overlay */

struct VSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 localPos : TEXCOORD0;
};

PSInput VS(VSInput input)
{
    PSInput output;

    float2 localPos = float2(input.position.x, input.position.y) * 2.0f;
    float2 pixelOffset = localPos * OverlayRadius;
    float2 screenPos = OverlayCenterScreen + pixelOffset;

    float2 ndc;
    ndc.x = (screenPos.x / ViewportSize.x) * 2.0f - 1.0f;
    ndc.y = 1.0f - (screenPos.y / ViewportSize.y) * 2.0f;

    output.position = float4(ndc, 0, 1);
    output.localPos = localPos;

    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float dist = length(input.localPos);

    if (dist > 1.0f)
        discard;

    return OverlayColor;
}
