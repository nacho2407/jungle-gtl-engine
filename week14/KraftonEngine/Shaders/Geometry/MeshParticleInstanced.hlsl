#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D DiffuseTexture : register(t0);

PS_Input_ColorTex VS(VS_Input_PNCTT_Instanced input)
{
    PS_Input_ColorTex output;
    
    float4x4 world = float4x4(
        input.world0,
        input.world1,
        input.world2,
        input.world3);
    
    float4 worldPos = mul(float4(input.position, 1.0f), world);
    
    output.position = ApplyVP(worldPos.xyz);
    output.color = input.color * input.instanceColor;
    output.texcoord = input.texcoord;

    return output;
}

float4 PS(PS_Input_ColorTex input) : SV_TARGET
{
    float4 tex = DiffuseTexture.Sample(LinearClampSampler, input.texcoord);
    return tex * input.color;
}
