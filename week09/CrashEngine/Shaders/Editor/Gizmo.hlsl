
/*
    Gizmo.hlsl는 에디터 오버레이와 보조 렌더링을 처리합니다.

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

#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"

cbuffer GizmoParams : register(b2)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
    uint AxisMask;
    uint3 _gizmoPad;
};

uint GetAxisFromColor(float3 color)
{
    if (color.g >= color.r && color.g >= color.b)
        return 1;
    if (color.b >= color.r && color.b >= color.g)
        return 2;
    return 0;
}

PS_Input_Color VS(VS_Input_PC input)
{
    PS_Input_Color output;
    output.position = ApplyMVP(input.position);
    output.color = input.color * GizmoColorTint;
    return output;
}

float4 PS(PS_Input_Color input) : SV_TARGET
{
    uint axis = GetAxisFromColor(input.color.rgb);

    if (!(AxisMask & (1u << axis)))
    {
        discard;
    }

    float4 outColor = input.color;

    if (axis == SelectedAxis)
    {
        outColor.rgb = float3(1, 1, 0);
        outColor.a = 1.0f;
    }

    if ((bool) bIsInnerGizmo)
    {
        outColor.a *= HoveredAxisOpacity;
    }

    if (axis != SelectedAxis && bClicking)
    {
        discard;
    }

    return saturate(outColor);
}

