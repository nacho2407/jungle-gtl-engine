
/*
    ShaderBillboard.hlsl는 에디터 오버레이와 보조 렌더링을 처리합니다.

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

#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"
#include "../Common/Resources/SystemSamplers.hlsl"

Texture2D BillboardTex : register(t0);

PS_Input_Tex VS(VS_Input_PNCT input)
{
    PS_Input_Tex output;
    output.position = ApplyMVP(input.position);
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float4 col = BillboardTex.Sample(LinearClampSampler, input.texcoord);

    if (!bIsWireframe && col.a < 0.05f)
        discard;

    return float4(ApplyWireframe(col.rgb), bIsWireframe ? 1.0f : col.a);
}

