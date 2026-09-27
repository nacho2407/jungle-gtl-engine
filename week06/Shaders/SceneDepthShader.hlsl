#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

Texture2D DepthTexture : register(t0);
SamplerState Sampler : register(s0);

cbuffer SceneDepthPostProcessCB : register(b3)
{
    float NearClip;
    float FarClip;
    uint bIsOrtho;
    float Exponent;
};

PS_Input_Tex VS(uint vertexID : SV_VertexID)
{
    PS_Input_Tex output;
    output.texcoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float depth = DepthTexture.Sample(Sampler, input.texcoord).r;
    
    if (1 - depth < 1e-4)
        return float4(0.0, 0.0, 0.0, 1.0f);
    
    float v;
    if (bIsOrtho != 0)
    {
        v = saturate(depth); 
    }
    else
    {
        float linZ = NearClip * FarClip / (FarClip - depth * (FarClip - NearClip));
        float visualFar = 100.0f; // 보여주고 싶은 최대 거리
        v = saturate((linZ - NearClip) / (visualFar - NearClip));
    }
    
    v = pow(v, Exponent);
    return float4(v, v, v, 1.0f);
}
