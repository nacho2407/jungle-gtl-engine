#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D DiffuseTexture : register(t0);

PS_Input_ColorTex VS(VS_Input_PCUV input)
{
    PS_Input_ColorTex output;
    output.position = ApplyVP(input.position);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_ColorTex input) : SV_TARGET
{
    float4 tex = DiffuseTexture.Sample(LinearClampSampler, input.texcoord);
    return tex * input.color;
}
