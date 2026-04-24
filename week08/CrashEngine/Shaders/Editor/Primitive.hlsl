
/*
    Primitive.hlsl는 에디터 오버레이와 보조 렌더링을 처리합니다.

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
    - 이 파일에서 직접 선언한 슬롯: b2, b3
*/

#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"

cbuffer PrimitiveColorParams : register(b2)
{
    float4 DiffuseColor;
};

cbuffer PrimitiveLightingParams : register(b3)
{
    float3 lightDir;
};

PS_Input_Color VS(VS_Input_PC input)
{
    PS_Input_Color output;
    output.position = ApplyMVP(input.position);
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_Color input) : SV_TARGET
{
    return float4(ApplyWireframe(input.color.rgb), input.color.a) * DiffuseColor;
}

