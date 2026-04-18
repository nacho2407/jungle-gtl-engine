// OutlinePostProcess.hlsl
// Fullscreen Quad VS (SV_VertexID) + Stencil Edge Detection PS

#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"

// b2 (PerShader0): Outline 설정
cbuffer OutlinePostProcessCB : register(b2)
{
    float4 OutlineColor; // 아웃라인 색상 + 알파
    float OutlineThickness; // 샘플링 오프셋 (픽셀 단위, 보통 1.0)
    float3 _Pad;
};

// StencilTexture (t19) is declared in Common/SystemResources.hlsli

// ── VS: Fullscreen Triangle (vertex buffer 없이 SV_VertexID로 생성) ──
PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

// ── PS: Stencil Edge Detection ──
float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    int offset = max((int) OutlineThickness, 1);

    // 중심 스텐실 값
    uint center = StencilTexture.Load(int3(coord, 0)).g;

    // 선택된 오브젝트 영역 밖이면 스킵
    if (center == 0)
        discard;

    // 상하좌우 이웃 샘플링
    uint up = StencilTexture.Load(int3(coord + int2(0, -offset), 0)).g;
    uint down = StencilTexture.Load(int3(coord + int2(0, offset), 0)).g;
    uint left = StencilTexture.Load(int3(coord + int2(-offset, 0), 0)).g;
    uint right = StencilTexture.Load(int3(coord + int2(offset, 0), 0)).g;

    // 이웃 중 하나라도 0이면 → 경계(edge)
    if (up != 0 && down != 0 && left != 0 && right != 0)
        discard;

    return OutlineColor;
}
