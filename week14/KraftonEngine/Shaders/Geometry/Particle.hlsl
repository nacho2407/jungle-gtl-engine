#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"

PS_Input_Color VS(VS_Input_PC input)
{
    PS_Input_Color output;
    output.position = ApplyVP(input.position);
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_Color input) : SV_TARGET
{
    return input.color;
}
