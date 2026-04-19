#include "Common/Functions.hlsl"

struct FDepthOnlyVSOutput
{
    float4 position : SV_POSITION;
};

FDepthOnlyVSOutput VS(VS_Input_PNCT_T input)
{
    FDepthOnlyVSOutput output;
    output.position = ApplyMVP(input.position);
    return output;
}

float4 PS(FDepthOnlyVSOutput input) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
