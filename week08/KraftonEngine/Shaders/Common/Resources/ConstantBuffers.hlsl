
/*
    ConstantBuffers.hlsl는 공용 GPU 리소스 슬롯 선언을 제공합니다.

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
    - 이 파일에서 직접 선언한 슬롯: b0, b1, b4
*/

#ifndef CONSTANT_BUFFERS_HLSL
#define CONSTANT_BUFFERS_HLSL

#pragma pack_matrix(row_major)

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

cbuffer PerObjectBuffer : register(b1)
{
    float4x4 Model;
    float4x4 NormalMatrix;
    float4 PrimitiveColor;
};

struct FAmbientLightInfo
{
    float3 Color; // 12B
    float Intensity;
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
}

struct FLocalLightInfo
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

