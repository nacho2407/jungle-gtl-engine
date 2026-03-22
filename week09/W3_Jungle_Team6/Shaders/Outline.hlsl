/* Constant Buffers */
#include "Common.hlsl"
struct VSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

/* Outline */
PSInput VS(VSInput input)
{
    PSInput output;
    float3 scaledPos;

    if ((bool) PrimitiveType)
    { // 3D (Cube 등)
        float3 signDir = sign(input.position);
        float3 offset = signDir * (OutlineOffset * OutlineInvScale);
        scaledPos = input.position + offset;
    }
    else
    { // 2D (Plane 등)
        scaledPos = input.position * (1.0f + OutlineOffset);
    }

    output.position = ApplyMVP(scaledPos);
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    return OutlineColor;
}