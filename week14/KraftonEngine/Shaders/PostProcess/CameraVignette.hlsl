#include "Common/Functions.hlsli"

cbuffer CameraVignetteCB : register(b2)
{
    float4 VignetteColor;
    float VignetteIntensity;
    float VignetteRadius;
    float VignetteSoftness;
    float _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;
    float2 centered = uv * 2.0f - 1.0f;
    
    float dist = length(centered);
    
    float outer = VignetteRadius + VignetteSoftness;
    float mask = smoothstep(VignetteRadius, outer, dist);
    float alpha = saturate(mask * VignetteIntensity * VignetteColor.a);
    
    return float4(VignetteColor.rgb, alpha);
}
