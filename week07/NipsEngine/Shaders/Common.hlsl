#ifndef COMMON_HLSL
#define COMMON_HLSL

/* Constant Buffers */

cbuffer FrameBuffer : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 CameraPosition;
    float Padding1;
    float bIsWireframe;
    float3 WireframeRGB;
}

cbuffer PerObjectBuffer : register(b1)
{
    row_major float4x4 Model;
    row_major float4x4 WorldInvTrans;
    float4 PrimitiveColor; 
};

// b2는 shader별 material/pass-local constant buffer

struct FAmbientLightInfo
{
    float4 Color;
    float Intensity;
    float3 Padding0;
};

struct FDirectionalLightInfo
{
    float3 Direction;
    float Padding1;
    float4 Color;
    float Intensity;
    float3 Padding2;
};

struct FPointLightInfo
{
    float3 Position;
    float Intensity;

    float4 Color;

    float Radius;
    float FalloffExponent;
    float2 Padding4;
};

struct FSpotLightInfo
{
    float3 Position;
    float Intensity;

    float3 Direction;
    float Radius;

    float4 Color;

    float InnerConeCos;
    float OuterConeCos;
    float FalloffExponent;
    float Padding5;
};

cbuffer GlobalLighting : register(b3)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
    uint PointLightCount;
    uint SpotLightCount;
    float2 Padding3;
};

StructuredBuffer<FPointLightInfo> PointLights : register(t10);
StructuredBuffer<FSpotLightInfo> SpotLights : register(t11);

#define MAX_FOG_LAYER_COUNT 32

struct FogLayerData
{
    float4 FogColor;
    float FogDensity;
    float HeightFalloff;
    float FogHeight;
    float FogStartDistance;
    float FogCutoffDistance;
    float FogMaxOpacity;
    float2 FogPadding;
};

cbuffer FogBuffer : register(b9)
{
    uint FogLayerCount;
    float3 FogBufferPadding;
    FogLayerData FogLayers[MAX_FOG_LAYER_COUNT];
};

float4 ApplyMVP(float3 pos)
{
    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);
}

float3x3 Inverse3x3(float3x3 m)
{
    float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
              - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
              + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

    float invDet = 1.0 / det;

    float3x3 result;
    result[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
    result[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * invDet;
    result[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
    result[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * invDet;
    result[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
    result[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * invDet;
    result[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
    result[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * invDet;
    result[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;
    return result;
}

#endif
