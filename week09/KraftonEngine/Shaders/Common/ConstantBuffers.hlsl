#ifndef CONSTANT_BUFFERS_HLSL
#define CONSTANT_BUFFERS_HLSL

#pragma pack_matrix(row_major)

// b0: 프레임 공통 — ViewProj, 와이어프레임 설정
cbuffer FrameBuffer : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float4x4 InvViewProj;
    float bIsWireframe;
    float3 WireframeRGB;
    float Time;
    float3 CameraWorldPos;
}

// b1: 오브젝트별 — 월드 변환, 색상
cbuffer PerObjectBuffer : register(b1)
{
    float4x4 Model;
    float4x4 NormalMatrix;
    float4 PrimitiveColor;
};

// b4: 글로벌 라이트 — C++ FGlobalLightConstants와 1:1 대응
// Ambient + Directional 배열. LocalLights는 t6 StructuredBuffer로 별도 전달.
struct FAmbientLightInfo
{
    float3 Color; // 12B
    float Intensity; // 4B  → 16B
};

struct FDirectionalLightInfo
{
    float3 Color; // 12B
    float Intensity; // 4B
    float3 Direction; // 12B
    float Padding; // 4B
};

#define MAX_DIRECTIONAL_LIGHTS 4

cbuffer GlobalLightBuffer : register(b4)
{
    FAmbientLightInfo Ambient; // 16B
    FDirectionalLightInfo Directional[MAX_DIRECTIONAL_LIGHTS]; // 128B
    int NumDirectionalLights;  // 4B
    int NumLocalLights;        // 4B
    float2 Padding;            // 8B
    // total: 160B — C++ FGlobalLightConstants과 일치
}

// t6: LocalLights StructuredBuffer — C++ FLocalLightInfo와 1:1 대응
struct FLocalLightInfo
{
    float3 Color; // 12B
    float Intensity; // 4B
    float3 Position; // 12B
    float AttenuationRadius; // 4B
    float3 Direction; // 12B
    float InnerConeAngle; // 4B (라디안)
    float OuterConeAngle; // 4B (라디안)
    float3 Padding; // 12B
    // total: 64B
};

#endif // CONSTANT_BUFFERS_HLSL
