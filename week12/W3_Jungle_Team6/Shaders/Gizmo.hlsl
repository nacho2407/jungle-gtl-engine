/* Constant Buffers */

#include "Common.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

uint GetAxisFromColor(float3 color)
{
    if (color.g >= color.r && color.g >= color.b)
        return 1;
    if (color.b >= color.r && color.b >= color.g)
        return 2;
    return 0;
}

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = ApplyMVP(input.position);
    output.color = input.color * GizmoColorTint;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    uint axis = GetAxisFromColor(input.color.rgb);
    float4 outColor = input.color;

    if (axis == SelectedAxis)
    {
        outColor.rgb = float3(1, 1, 0);
        outColor.a = 1.0f;
    }
    
    if ((bool) bIsInnerGizmo)
    {
        outColor.a *= HoveredAxisOpacity;
    }
    
    if (axis != SelectedAxis && bClicking)
    {
        discard;
    }

    return saturate(outColor);
}
