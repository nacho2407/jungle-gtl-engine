#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"
#include "common/Functions.hlsl"

Texture2D DepthTexture : register(t1);
SamplerState g_Sample : register(s0);

struct PS_Input_Decal
{
    float4 position : SV_POSITION;
    float4 clipPos : TEXCOORD0;
};

PS_Input_Decal VS(uint vid : SV_VertexID)
{
    PS_Input_Decal output;
    
    float2 pos = float2((vid == 2) ? 3.0f : -1.0f, (vid == 1) ? 3.0f : -1.0f);
    output.position = float4(pos, 0.0f, 1.0f);
    output.clipPos = output.position;

    return output;
}

float4 PS(PS_Input_Decal input) : SV_TARGET
{
    float2 ndc = input.clipPos.xy / input.clipPos.w;
    float2 uv = ndc * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;

    float depth = DepthTexture.Sample(g_Sample, uv).r;

    float4 clip = float4(ndc, depth, 1.0f);
    float4 viewPos = mul(clip, InvProjMatrix);
    viewPos /= viewPos.w;

    float4 worldPos4 = mul(viewPos, InvViewMatrix);
    float3 worldPos = worldPos4.xyz;

    float3 fireballPos = Model[3].xyz;
    float dist = length(worldPos - fireballPos);

    float distMask = step(dist, Radius);
    if (distMask <= 0.0f)
        discard;

    float falloff = saturate(1.0f - (dist / (Radius)));
    falloff = pow(falloff, max(RadiusFalloff, 0.01f));

    float3 rgb = Color.rgb * Intensity * falloff;
    float alpha = Color.a * Intensity * falloff;

    return float4(rgb, alpha);
}