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
#define SHADOW_ESM_EXPONENT 10.0f
#endif

struct FShadowEncodeVSOutput
{
    float4 Position : SV_POSITION;
    float3 ViewPos  : TEXCOORD0;
};

FShadowEncodeVSOutput VS(VS_Input_PNCT_T Input)
{
    FShadowEncodeVSOutput Output;
    float4 World = mul(float4(Input.position, 1.0f), Model);
    float4 ShadowViewPosition = mul(World, ShadowView);
    Output.Position = mul(ShadowViewPosition, ShadowProjection);
    Output.ViewPos = ShadowViewPosition.xyz;
    return Output;
}

float2 PS(FShadowEncodeVSOutput Input) : SV_Target0
{
    const float DepthRange = max(ShadowFarZ - ShadowNearZ, 1e-5f);
    const float D = saturate((Input.ViewPos.z - ShadowNearZ) / DepthRange);

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
