#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D BloomTexture : register(t26);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 scene = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float3 bloom = BloomTexture.SampleLevel(LinearClampSampler, input.uv, 0).rgb;

    float intensity = 0.45f;
    scene.rgb += bloom * intensity;

    return scene;
}
