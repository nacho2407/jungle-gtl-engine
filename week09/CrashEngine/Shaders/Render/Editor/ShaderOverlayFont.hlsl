/*
    ShaderOverlayFont.hlsl는 에디터 오버레이와 보조 렌더링을 처리합니다.

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
    - 이 파일에서 직접 선언한 슬롯: t0
*/

#include "../../Common/VertexLayouts.hlsl"
#include "../../Resources/SystemSamplers.hlsl"

Texture2D FontAtlas : register(t0);

PS_Input_Tex VS(VS_Input_PT input)
{
    PS_Input_Tex output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float4 col = FontAtlas.Sample(PointClampSampler, input.texcoord);

    if (col.r < 0.1f)
        discard;

    return float4(0.6f, 1.0f, 1.0f, col.r);
}

