/*
    ShadowEncodePass.hlsl
    Shadow map generation pass that writes filterable shadow data to color RT.
*/

#include "../../../Utils/Functions.hlsl"

#define SHADOW_FILTER_METHOD_PCF 0
#define SHADOW_FILTER_METHOD_VSM 1
#define SHADOW_FILTER_METHOD_ESM 2

#ifndef SHADOW_FILTER_METHOD
#define SHADOW_FILTER_METHOD SHADOW_FILTER_METHOD_PCF
#endif

#ifndef SHADOW_ESM_EXPONENT
#define SHADOW_ESM_EXPONENT 80.0f
#endif

struct FShadowEncodeVSOutput
{
    float4 Position : SV_POSITION;
};

FShadowEncodeVSOutput VS(VS_Input_PNCT_T Input)
{
    FShadowEncodeVSOutput Output;
    Output.Position = ApplyMVP(Input.position);
    return Output;
}

float2 PS(FShadowEncodeVSOutput Input) : SV_Target0
{
    const float D = Input.Position.z;

#if SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_VSM
    const float Dx = ddx(D);
    const float Dy = ddy(D);
    const float Variance = 0.25f * (Dx * Dx + Dy * Dy);
    return float2(D, D * D + Variance);
#elif SHADOW_FILTER_METHOD == SHADOW_FILTER_METHOD_ESM
    const float Encoded = exp(-SHADOW_ESM_EXPONENT * D);
    return float2(Encoded, 0.0f);
#else
    return float2(D, D * D);
#endif
}
