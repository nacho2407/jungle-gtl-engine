#include "Common/Functions.hlsli"

cbuffer CameraFadeCB : register(b2)
{
    float4 FadeColor;
    float FadeAmount;
    float3 _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    return float4(FadeColor.rgb, FadeColor.a * FadeAmount);
}
