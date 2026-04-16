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
//float4 PS(PS_Input input) : SV_TARGET
//{
//    int2 coord = int2(input.position.xy);
//    int offset = max((int) OutlineThickness, 1);

//    // 중심 스텐실 값
//    uint center = StencilTex.Load(int3(coord, 0)).g;

//    // 선택된 오브젝트 영역 밖이면 스킵
//    if (center == 0)
//        discard;

//    // 상하좌우 이웃 샘플링
//    uint up = StencilTex.Load(int3(coord + int2(0, -offset), 0)).g;
//    uint down = StencilTex.Load(int3(coord + int2(0, offset), 0)).g;
//    uint left = StencilTex.Load(int3(coord + int2(-offset, 0), 0)).g;
//    uint right = StencilTex.Load(int3(coord + int2(offset, 0), 0)).g;

//    // 이웃 중 하나라도 0이면 → 경계(edge)
//    if (up != 0 && down != 0 && left != 0 && right != 0)
//        discard;

//    return OutlineColor;
//}

// ── PS: Improved Stencil Edge Detection (Outer Outline) ──
float4 PS(PS_Input input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    uint center = StencilTex.Load(int3(coord, 0)).g;

    // 1. 오브젝트 '안쪽' 픽셀은 아웃라인을 덮어씌우지 않고 원본을 보여주기 위해 스킵
    // (만약 반투명한 아웃라인이 오브젝트를 덮는 걸 원한다면 이 줄을 지우세요)
    if (center != 0) 
        discard;

    int radius = max((int) OutlineThickness, 1);
    bool bIsEdge = false;

    // 2. 주변 픽셀을 탐색하여 스텐실 값이 0이 아닌(오브젝트인) 픽셀이 하나라도 있는지 확인
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            // 중심은 이미 위에서 검사했으므로 패스
            if (x == 0 && y == 0)
                continue;

            // 원형(Circle)으로 외곽선을 부드럽게 만들고 싶다면 아래 주석 해제 (성능 약간 하락)
            // if (x*x + y*y > radius*radius) continue;

            uint neighbor = StencilTex.Load(int3(coord + int2(x, y), 0)).g;
            if (neighbor != 0)
            {
                bIsEdge = true;
                break;
            }
        }
        if (bIsEdge)
            break;
    }

    // 주변에 오브젝트 픽셀이 없다면 완전한 허공이므로 스킵
    if (!bIsEdge)
        discard;

    return OutlineColor;
}