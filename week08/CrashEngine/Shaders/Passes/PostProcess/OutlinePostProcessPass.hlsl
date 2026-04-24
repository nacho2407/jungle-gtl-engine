
/*
    OutlinePostProcessPass.hlsl는 후처리 렌더 패스의 셰이더입니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
    - 이 파일에서 직접 선언한 슬롯: b2
*/

// Fullscreen Triangle VS + stencil-based outline PS.

#include "../../Common/Utils/Functions.hlsl"
#include "../../Common/Resources/SystemResources.hlsl"

cbuffer OutlinePostProcessCB : register(b2)
{
    float4 OutlineColor;
    float OutlineThickness;
    float3 _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    const int2 coord = int2(input.position.xy);
    const int radius = min(max((int)(OutlineThickness + 0.5f), 1), 6);

    const uint center = StencilTex.Load(int3(coord, 0)).g;

    // Draw the outline on background pixels adjacent to selected geometry.
    if (center != 0)
        discard;

    uint neighborMask = 0;
    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            if (x == 0 && y == 0)
                continue;

            neighborMask |= StencilTex.Load(int3(coord + int2(x, y), 0)).g;
        }
    }

    if (neighborMask == 0)
        discard;

    return OutlineColor;
}

