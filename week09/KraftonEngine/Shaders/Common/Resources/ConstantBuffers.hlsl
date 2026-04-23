// Shader include: Common/Resources/ConstantBuffers.hlsl
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

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
    float Intensity; // 4B  ??16B
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

