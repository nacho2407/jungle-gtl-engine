#ifndef GIZMO_BUFFER_HLSL
#define GIZMO_BUFFER_HLSL

// b2: 기즈모 전용
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

#endif // GIZMO_BUFFER_HLSL
