#ifndef CONSTANT_BUFFERS_HLSL
#define CONSTANT_BUFFERS_HLSL

#pragma pack_matrix(row_major)

// b0: 프레임 공통 — ViewProj, 와이어프레임 설정
cbuffer FrameBuffer : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float4x4 InvProj;
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

// 시스템 샘플러 (s0~s4)
#include "Common/SystemSamplers.hlsli"

// b5: Shadow 행렬 + 파라미터
#define MAX_SHADOW_CASCADES 4

cbuffer ShadowBuffer : register(b5)
{
    float4x4 ShadowLightViewProj[MAX_SHADOW_CASCADES]; // Directional/CSM
    float4x4 ShadowPointLightViewProj[6];              // Point cubemap 6면
    float4x4 ShadowSpotLightViewProj;                  // Spot

    float4   CascadeSplits;                            // CSM cascade 분할 거리
    float4   ShadowAtlasScaleBias;                     // Atlas UV transform

    float    ShadowBias;
    float    ShadowSlopeBias;
    float    ShadowSharpen;
    uint     ShadowMapResolution;

    uint     NumCascades;
    uint     ShadowFilterMode;                         // 0=Hard, 1=PCF, 2=VSM
    float2   _shadowPad;
};

#endif // CONSTANT_BUFFERS_HLSL
