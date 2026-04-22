#include "../../Common/Utils/Functions.hlsl"

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

void PS(FDepthOnlyVSOutput input) {
    //return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
