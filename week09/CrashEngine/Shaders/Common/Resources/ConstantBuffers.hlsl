/*
    ConstantBuffers.hlsl는 공용 상수 버퍼 레이아웃과 슬롯 선언을 제공합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: GlobalLight 상수 버퍼
    - 이 파일에서 직접 선언하는 슬롯: b0, b1, b4
*/

#ifndef CONSTANT_BUFFERS_HLSL
#define CONSTANT_BUFFERS_HLSL

#pragma pack_matrix(row_major)

cbuffer FrameParams : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float4x4 InvViewProj;
    float bIsWireframe;
    float3 WireframeRGB;
    float Time;
    float3 CameraWorldPos;
}

cbuffer PerObjectParams : register(b1)
{
    float4x4 Model;
    float4x4 NormalMatrix;
    float4 PrimitiveColor;
};

struct FAmbientLight
{
    float3 Color; // 12B
    float Intensity;
};

struct FDirectionalLight
{
    float3 Color; // 12B
    float Intensity; // 4B
    float3 Direction; // 12B
    float Padding; // 4B
};

#define MAX_DIRECTIONAL_LIGHTS 4

cbuffer GlobalLightParams : register(b4)
{
    FAmbientLight Ambient; // 16B
    FDirectionalLight Directional[MAX_DIRECTIONAL_LIGHTS]; // 128B
    int NumDirectionalLights;  // 4B
    int NumLocalLights;        // 4B
    float2 Padding;            // 8B
}

struct FLocalLight
{
    float3 Color; // 12B
    float Intensity; // 4B
    float3 Position; // 12B
    float AttenuationRadius; // 4B
    float3 Direction; // 12B
    float InnerConeAngle;
    float OuterConeAngle;
    float3 Padding; // 12B
    // total: 64B
};

#endif // CONSTANT_BUFFERS_HLSL
