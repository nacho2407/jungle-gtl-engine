/* Constant Buffers */
#include "Common.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

/* Outline */
PSInput VS(VSInput input)
{
    PSInput output;
    float3 scaledPos = input.position + normalize(input.Normal) * OutlineFactor;
    
    // if ((bool) PrimitiveType)
    // { // 3D (Cube 등)
    //     float3 signDir = sign(input.position);
    //     float3 offset = signDir * (OutlineOffset * OutlineInvScale);
    //     scaledPos = input.position + offset;
    // }
    // else
    // { // 2D (Plane 등)
    //     float3 signDir = sign(input.position);
    //     float3 offset = signDir * (OutlineOffset * OutlineInvScale);
    //     
    //     scaledPos = input.position + offset;
    // }

    output.position = ApplyMVP(scaledPos);
    output.normal = input.Normal;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    // return float4(input.normal * 0.5f + 0.5f, 1.0f);
    return OutlineColor;
}

