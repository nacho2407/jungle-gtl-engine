#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D SourceTexture : register(t0);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    return SourceTexture.SampleLevel(LinearClampSampler, input.uv, 0);
}
