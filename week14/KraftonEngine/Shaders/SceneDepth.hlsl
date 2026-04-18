// SceneDepth.hlsl
#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"

// b2 (PerShader0): SceneDepth visualization
cbuffer SceneDepthCB : register(b2)
{
    float Exponent;
    float NearClip;
    float FarClip;
    uint Mode;
}

// SceneDepthTexture (t16) is declared in Common/SystemResources.hlsli

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    
    float d = SceneDepthTexture.Load(int3(coord, 0));
    
    float v = 0.0f;
    
    if (Mode == 1)
    {
        // Reversed-Z linearization: d=1 at near, d=0 at far
        float linZ = NearClip * FarClip / (NearClip - d * (NearClip - FarClip));
        v = saturate((linZ - NearClip) / (FarClip - NearClip));
    }
    else
    {
        // Reversed-Z: invert so near=dark, far=bright (matching Forward-Z visual)
        v = pow(saturate(1.0 - d), Exponent);
    }
    
    return float4(v, v, v, 1.0f);
}
