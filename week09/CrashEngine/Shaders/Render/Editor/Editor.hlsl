/*
    Editor.hlsl는 에디터 오버레이와 보조 렌더링을 처리합니다.

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
*/

#include "../../Resources/ConstantBuffers.hlsl"
#include "../../Common/VertexLayouts.hlsl"

PS_Input_ColorWorld VS(VS_Input_PC input)
{
    PS_Input_ColorWorld output;

    float3 worldPos = input.position.xyz;
    output.worldPos = worldPos;

    float4 viewPos = mul(float4(worldPos, 1.0f), View);
    output.position = mul(viewPos, Projection);

    output.color = input.color;

    return output;
}

float4 PS(PS_Input_ColorWorld input) : SV_TARGET
{
    return input.color;
}

