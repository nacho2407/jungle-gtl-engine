#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"
#include "common/Functions.hlsl"

Texture2D g_txColor : register(t0);
Texture2D DepthTexture : register(t1);
SamplerState g_Sample : register(s0);

struct PS_Input_Decal
{
    float4 position : SV_POSITION;
    float4 clipPos : TEXCOORD0;
};

PS_Input_Decal VS(VS_Input_PC input)
{
    PS_Input_Decal output;
    
    float4 clip = ApplyMVP(input.position);

    output.position = clip;
    output.clipPos = clip;

    return output;
}

float4 PS(PS_Input_Decal input) : SV_TARGET
{
    float2 ndc = input.clipPos.xy / input.clipPos.w;
    float2 uv = ndc * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;

    float depth = DepthTexture.Sample(g_Sample, uv).r;
    if (depth <= 0.0f)
    {
        discard;
    }
    
    float4 clip = float4(ndc, depth, 1.0f);
    float4 viewPos = mul(clip, InvProjMatrix);
    viewPos /= viewPos.w;

    float4 worldPos4 = mul(viewPos, InvViewMatrix);
    float3 worldPos = worldPos4.xyz;
    
    float3 fireballPos = Model[3].xyz;
    float distance = length(worldPos - fireballPos);
    if(distance > Radius)
    {
        discard;
    }
    
    float RadiusFalloff = saturate(1.0f - (distance / max(Radius, 0.0001f)));
    RadiusFalloff = pow(RadiusFalloff, max(RadiusFalloff, 0.0001f));
    
    float falloff = saturate(1.0f - (distance / max(Radius, 0.0001f)));
    falloff = pow(falloff, max(RadiusFalloff, 0.0001f));

    float alpha = Color.a * Intensity * falloff;
    if (alpha <= 0.001f)
    {
        discard;
    }

    float3 rgb = Color.rgb * Intensity * falloff;
    return float4(rgb, alpha);
}