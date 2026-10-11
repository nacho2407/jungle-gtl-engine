// SceneNormal.hlsl — GBuffer World Normal 시각화
#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);

    float4 normalData = GBufferNormalTexture.Load(int3(coord, 0));

    // alpha == 0이면 기록된 노말 없음 (배경)
    if (normalData.a < 0.5f)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    // World Normal [-1,1] → [0,1] 색상 매핑
    float3 N = normalize(normalData.xyz);
    return float4(N * 0.5f + 0.5f, 1.0f);
}
