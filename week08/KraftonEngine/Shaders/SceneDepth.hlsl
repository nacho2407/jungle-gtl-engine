// SceneDepth.hlsl
#include "Common/Functions.hlsl"

// b2 (PerShader0): SceneDepth visualization
cbuffer SceneDepthCB : register(b2)
{
    float Exponent;
    float NearClip;
    float FarClip;
    uint Mode;
}

Texture2D<float> DepthTex : register(t0);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    
    float d = DepthTex.Load(int3(coord, 0));
    
    float v = 0.0f;
    
    if (Mode == 1)
    {
        float linZ = NearClip * FarClip / (FarClip - d * (FarClip - NearClip));
        v = saturate((linZ - NearClip) / (FarClip - NearClip));
    }
    else
    {
        v = pow(saturate(d), Exponent);
    }
    
    return float4(v, v, v, 1.0f);
}