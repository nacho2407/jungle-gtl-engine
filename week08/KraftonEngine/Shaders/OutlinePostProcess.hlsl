// OutlinePostProcess.hlsl
// Fullscreen Quad VS (SV_VertexID) + Stencil Edge Detection PS

#include "Common/ConstantBuffers.hlsl"

// StencilSRV: X24_TYPELESS_G8_UINT 포맷 → uint2, 스텐실은 .g 채널
Texture2D<uint2> StencilTex : register(t0);

// ── VS: Fullscreen Triangle (vertex buffer 없이 SV_VertexID로 생성) ──
struct PS_Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_Input VS(uint vertexID : SV_VertexID)
{
    // 삼각형 하나로 화면 전체 커버 (0,1,2 → 오버사이즈 삼각형)
    PS_Input output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

// ── PS: Stencil Edge Detection ──
float4 PS(PS_Input input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    int offset = max((int) OutlineThickness, 1);

    // 중심 스텐실 값
    uint center = StencilTex.Load(int3(coord, 0)).g;

    // 선택된 오브젝트 영역 밖이면 스킵
    if (center == 0)
        discard;

    // 상하좌우 이웃 샘플링
    uint up = StencilTex.Load(int3(coord + int2(0, -offset), 0)).g;
    uint down = StencilTex.Load(int3(coord + int2(0, offset), 0)).g;
    uint left = StencilTex.Load(int3(coord + int2(-offset, 0), 0)).g;
    uint right = StencilTex.Load(int3(coord + int2(offset, 0), 0)).g;

    // 이웃 중 하나라도 0이면 → 경계(edge)
    if (up != 0 && down != 0 && left != 0 && right != 0)
        discard;

    return OutlineColor;
}
