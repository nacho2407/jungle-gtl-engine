#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/MaterialBuffer.hlsl"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

cbuffer DecalBuffer : register(b5)
{
    float4 DecalColor;
}

PS_Input_Full VS(VS_Input_PNCT input)
{
    PS_Input_Full output;
    output.position = ApplyMVP(input.position);
    output.normal = normalize(mul(input.normal, (float3x3) Model));
    output.color = input.color * SectionColor;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Full input) : SV_TARGET
{
    float4 texColor = g_txColor.Sample(g_Sample, input.texcoord);
    
    float4 finalColor = texColor * input.color * DecalColor;
    return float4(ApplyWireframe(finalColor.rgb), finalColor.a);
}
