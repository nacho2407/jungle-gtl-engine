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

// b5: Shadow CB — CSM 행렬 + 공통 파라미터
// Per-light 데이터는 StructuredBuffer (t24, t25)로 분리
#define MAX_SHADOW_CASCADES     4
#define MAX_SHADOW_SPOT_LIGHTS  64
#define MAX_SHADOW_POINT_LIGHTS 16

cbuffer ShadowBuffer : register(b5)
{
    // Directional CSM
    float4x4 CSMViewProj[MAX_SHADOW_CASCADES]; // 256B
    float4   CascadeSplits;                    //  16B

    // CSM(Directional) 파라미터 — Spot/Point는 per-light StructuredBuffer(t24,t25) 참조
    float    ShadowBias;                       //   4B
    float    ShadowSlopeBias;                  //   4B
    float    ShadowNormalBias;                 //   4B    
    float    ShadowSharpen;                    //   4B
    
    uint     ShadowFilterMode;                 //   4B  (0=Hard, 1=PCF, 2=VSM)
    uint     NumCSMCascades;                   //   4B
    uint     NumShadowSpotLights;              //   4B
    uint     NumShadowPointLights;             //   4B
    
    uint     CSMResolution;                    //   4B
    float    CSMBlendRange;                    //   4B
    uint     CSMBlendEnabled;                  //   4B
    uint     SpotAtlasResolution;              //   4B

    uint     PointAtlasResolution;             //   4B
    float    _SBPad[3];                        //  12B → 합계 336B
};

// ── Shadow 텍스처 바인딩 ──
// t21: Directional CSM (4 cascades)
Texture2DArray    ShadowMapCSM       : register(t21);
// t22: Spot Light Atlas (page = slice, 내부 UV rect packing)
Texture2DArray    ShadowMapSpotAtlas : register(t22);
// t23: Point Light Atlas (Texture2DArray, page-based multi-light packing)
Texture2DArray ShadowMapPointLightAtlas : register(t23);

#endif // CONSTANT_BUFFERS_HLSL
