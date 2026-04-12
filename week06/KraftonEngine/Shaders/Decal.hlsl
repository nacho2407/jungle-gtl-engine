#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

Texture2D g_txDecal : register(t0);
SamplerState g_sampler : register(s0);

struct PS_Input_Decal
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

PS_Input_Decal VS(VS_Input_PNCT input)
{
    PS_Input_Decal output;

    float4 worldPos = mul(float4(input.position, 1.0f), Model);
    float4 viewPos = mul(worldPos, View);

    output.position = mul(viewPos, Projection);
    output.worldPos = worldPos.xyz;
    return output;
}

float4 PS(PS_Input_Decal input) : SV_TARGET
{
    float3 decalLocalPos = mul(float4(input.worldPos, 1.0f), WorldToDecal).xyz;

    if (abs(decalLocalPos.x) > DecalHalfExtents.x ||
        abs(decalLocalPos.y) > DecalHalfExtents.y ||
        abs(decalLocalPos.z) > DecalHalfExtents.z)
    {
        discard;
    }

    float2 decalUV;
    decalUV.x = decalLocalPos.y / (DecalHalfExtents.y * 2.0f) + 0.5f;
    decalUV.y = 0.5f - (decalLocalPos.z / (DecalHalfExtents.z * 2.0f));

    float4 decalColor = g_txDecal.Sample(g_sampler, decalUV);
    if (bIsWireframe || decalColor.a < 1e-6f)
    {
        discard;
    }
    
    decalColor.a *= FadeAlpha;
    decalColor.rgb = ApplyWireframe(decalColor.rgb);
    return decalColor;
}
