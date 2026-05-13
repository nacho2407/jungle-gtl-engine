/* Constant Buffers */
#ifndef COMMON_H
#define COMMON_H

#define TILE_SIZE 16
#define NUM_SLICE 24

#define MAX_ATLAS_SHADOW_COUNT 64
#define MAX_DIRECTIONAL_CASCADE_COUNT 4
#define INVALID_SHADOW_INDEX 0xFFFFFFFFu

cbuffer FrameBuffer : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 CameraPosition;
    float IsOrthographic;
    float3 WireframeRGB;
    float bIsWireframe;
    float2 ViewportSize;
    float NearZ;
    float FarZ;
}

cbuffer PerObjectBuffer : register(b1)
{
    row_major float4x4 Model;
    row_major float4x4 WorldInvTrans;
    float4 PrimitiveColor; 
};

cbuffer ShadowBuffer : register(b4)
{
    row_major matrix VirtualViewProj;
    row_major matrix ShadowViewProj;
    
    float4 CascadeSplitFar;
    uint DirectionalCascadeCount;
    uint DirectionalShadowStartIndex;
    float2 ShadowBufferPadding;
};

struct FLightShadowIndices
{
    uint ShadowIndex;
    uint IndexCount;
};

struct FAtlasShadowData
{
    row_major float4x4 ShadowViewProj;
    row_major float4x4 VirtualViewProj;

    float4 ScaleOffset;
    float ConstantBias;
    float ShadowStrength;
    float ShadowSoftness;
    uint ShadowType;

    uint ShadowMapType;
    float SlopedBias;
    float2 Padding;
};

#ifndef CS_SHADER
StructuredBuffer<FLightShadowIndices> LightShadowIndices : register(t14);
StructuredBuffer<FAtlasShadowData> AtlasShadowDatas : register(t15);
#endif

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

float LinearizeDepth(float d)
{
    if (IsOrthographic > 0.5f)
    {
        return NearZ + d * (FarZ - NearZ);
    }
    else
    {
        return (NearZ * FarZ) / (FarZ - d * (FarZ - NearZ));
    }
}

#endif
