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
    float4 viewPos = mul(clip, InvProj);
    viewPos /= viewPos.w;

    float4 worldPos = mul(viewPos, InvView);

    float4 localPos4 = mul(worldPos, DecalWorldToLocal);
    float3 localPos = localPos4.xyz;

    if (abs(localPos.x) > 0.5f || abs(localPos.y) > 0.5f || abs(localPos.z) > 0.5f)
    {
        discard;
    }

    float2 decalUV = localPos.yz + 0.5f;
    decalUV.y *= -1;
    
    float4 decalColor = g_txColor.Sample(g_Sample, decalUV);
    decalColor.rgb *= DecalColor.rgb;
    decalColor.a *= DecalOpacity;

    if (decalColor.a <= 0.001f)
    {
        discard;
    }

    return decalColor;
}