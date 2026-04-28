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
    float4x4 View;         // 64B
    float4x4 Projection;   // 64B
    float4x4 InvViewProj;  // 64B
    float bIsWireframe;    // 4B
    float3 WireframeRGB;   // 12B
    float Time;            // 4B
    float3 CameraWorldPos; // 12B
}; // Total: 224B

cbuffer PerObjectParams : register(b1)
{
    float4x4 Model;                    // 64B
    float4x4 NormalMatrix;             // 64B
    float4 PrimitiveColor;             // 16B
    uint PrimitiveDecalIndexOffset;    // 4B
    uint PrimitiveDecalCount;          // 4B
    float2 PrimitivePerObjectPadding;  // 8B
}; // Total: 160B

struct FAmbientLight
{
    float3 Color;    // 12B
    float Intensity; // 4B
}; // Total: 16B

struct FDirectionalLight
{
    float3 Color;                  // 12B
    float Intensity;               // 4B
    float3 Direction;              // 12B
    int CascadeCount;              // 4B
    float4x4 ShadowViewProj[4];    // 256B
    float4 ShadowSampleData[4][2]; // 128B
    float ShadowBias;              // 4B
    float ShadowSlopeBias;         // 4B
    float ShadowNormalBias;        // 4B
    float _Padding0;               // 4B
    float4 CascadeSplits;          // 16B
}; // Total: 448B

#define MAX_DIRECTIONAL_LIGHTS 4

cbuffer GlobalLightParams : register(b4)
{
    FAmbientLight Ambient;                             // 16B
    int NumDirectionalLights;                          // 4B
    int NumLocalLights;                                // 4B
    float2 _Padding0;                                  // 8B
    FDirectionalLight Directional[MAX_DIRECTIONAL_LIGHTS]; // 1792B
}; // Total: 1824B


// TODO: 라이트 타입에 따라 구조체 분리
struct FLocalLight
{
    uint LightType;                // 4B
    float3 Color;                  // 12B
    float Intensity;               // 4B
    float3 Position;               // 12B
    float AttenuationRadius;       // 4B
    float3 Direction;              // 12B
    float InnerConeAngle;          // 4B
    float OuterConeAngle;          // 4B
    int ShadowSampleCount;         // 4B
    float _Pad0;                   // 4B
    float4x4 ShadowViewProj[6];    // 384B
    float4 ShadowSampleData[6][2]; // 192B
    float ShadowBias;              // 4B
    float ShadowSlopeBias;         // 4B
    float ShadowNormalBias;        // 4B
    float _Pad1;                   // 4B
}; // Total: 656B

#endif // CONSTANT_BUFFERS_HLSL
