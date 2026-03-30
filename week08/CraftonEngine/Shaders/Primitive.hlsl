#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

PS_Input_Color VS(VS_Input_PC input)
{
    PS_Input_Color output;
    output.position = ApplyMVP(input.position);
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_Color input) : SV_TARGET
{
    return float4(ApplyWireframe(input.color.rgb), input.color.a);
}
