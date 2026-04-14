#ifndef CONSTANT_BUFFERS_HLSL
#define CONSTANT_BUFFERS_HLSL

#pragma pack_matrix(row_major)

// b0: 프레임 공통 — ViewProj, 와이어프레임 설정
cbuffer FrameBuffer : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float bIsWireframe;
    float3 WireframeRGB;
    float Time;
    float3 _framePad;
}

// b1: 오브젝트별 — 월드 변환, 색상
cbuffer PerObjectBuffer : register(b1)
{
    float4x4 Model;
    float4 PrimitiveColor;
};

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

// ── Outline 설정 (b3) ──
cbuffer OutlinePostProcessCB : register(b3)
{
    float4 OutlineColor; // 아웃라인 색상 + 알파
    float OutlineThickness; // 샘플링 오프셋 (픽셀 단위, 보통 1.0)
    float3 _Pad;
};

// b4: Material properties
cbuffer MaterialBuffer : register(b4)
{
    uint bIsUVScroll;
    float3 _matPad;
    float4 SectionColor;
}

// b5: Decal settings
cbuffer DecalBuffer : register(b5)
{
    float4x4 WorldToDecal;
    float3 DecalHalfExtents;
    float FadeAlpha;
    float _decalPad;
}

// b6: Height fog postprocess params
cbuffer HeightFogPostProcessCB : register(b6)
{
    float4x4 CameraInvView;
    float4x4 CameraInvProjection;
    float4 FogColor;
    float3 FogCameraPosition;
    float FogDensity;
    float FogHeight;
    float FogHeightFalloff;
    float FogStartDistance;
    float FogMaxOpacity;
    float FogEndDistance;
    float FogCutoffDistance;
    float2 FogPadding;
}

// b7: Fireball params
cbuffer FireballBuffer : register(b7)
{
    float4x4 InvViewMatrix;
    float4x4 InvProjMatrix;
    float4 Color;
    float Intensity;
    float Radius;
    float RadiusFalloff;
    float Padding[2];
}

#endif // CONSTANT_BUFFERS_HLSL
