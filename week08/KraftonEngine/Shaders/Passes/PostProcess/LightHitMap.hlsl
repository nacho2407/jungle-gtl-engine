// OutlinePostProcess.hlsl
// Fullscreen Triangle VS + stencil-based outline PS.

#include "../../Common/Utils/Functions.hlsl"
#include "../../Common/Resources/SystemResources.hlsl"
#include "../../Common/Resources/SystemSamplers.hlsl"

//Texture2D<float>  SceneDepth  : register(t10);  // SystemResources.hlsl
Texture2D g_DebugHitMapTex : register(t8);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);

    float4 BaseColor = SceneColor.Load(int3(coord, 0));
    return BaseColor + g_DebugHitMapTex.Sample(PointClampSampler, input.uv);
}
