#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"

// b2 (PerShader0): 기즈모 전용
cbuffer GizmoBuffer : register(b2)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
    uint AxisMask; // 비트 0=X, 1=Y, 2=Z
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

    // AxisMask 기반 축 숨김
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
