#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"

cbuffer PerShader1 : register(b2)
{
    float4 DiffuseColor;
};

cbuffer PerShader2 : register(b3)
{
    float3 lightDir;
};

PS_Input_Color VS(VS_Input_PC input)
{
    PS_Input_Color output;
    output.position = ApplyMVP(input.position);
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_Color input) : SV_TARGET
{
    return float4(ApplyWireframe(input.color.rgb), input.color.a) * DiffuseColor;
}
