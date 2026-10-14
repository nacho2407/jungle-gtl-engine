#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/Fog.hlsli"
#include "Common/MaterialBloom.hlsli"

Texture2D DiffuseTexture : register(t0);

PS_Input_Particle VS(VS_Input_PCUV input)
{
    PS_Input_Particle output;
    output.worldPos = input.position;
    output.position = ApplyVP(input.position);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Particle input) : SV_TARGET
{
    float4 tex = DiffuseTexture.Sample(LinearWrapSampler, input.texcoord);
    float4 color = tex * input.color;
    color.rgb = ApplyHeightFog(color.rgb, input.worldPos);
    color.rgb = ApplyMaterialEmissive(color.rgb);
    return color;
}
