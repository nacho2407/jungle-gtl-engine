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
};

// b3: 아웃라인 전용
cbuffer OutlineConstants : register(b3)
{
    float4 OutlineColor;
    float3 OutlineInvScale;
    float OutlineOffset;
    uint bIs3D; //  0 : 2D (Plane), 1 : 3D (Cube)
    float3 Padding4;
};

#endif // CONSTANT_BUFFERS_HLSL
