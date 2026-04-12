#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

Texture2D g_txDecal : register(t0);
SamplerState g_sampler : register(s0);

struct PS_Input_Decal
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
};

PS_Input_Decal VS(VS_Input_PNCT input)
{
    PS_Input_Decal output;

    float4 worldPos = mul(float4(input.position, 1.0f), Model);
    float4 viewPos = mul(worldPos, View);

    output.position = mul(viewPos, Projection);
    output.worldPos = worldPos.xyz;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Decal input) : SV_TARGET
{
    float4 decalColor = g_txDecal.Sample(g_sampler, input.texcoord);
    decalColor.a *= FadeAlpha;
    decalColor.rgb = ApplyWireframe(decalColor.rgb);
    return decalColor;
}
