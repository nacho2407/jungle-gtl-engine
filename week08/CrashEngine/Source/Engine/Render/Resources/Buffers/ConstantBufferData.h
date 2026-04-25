#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

// FPerObjectCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FPerObjectCBData
{
    FMatrix  Model;
    FMatrix  NormalMatrix;
    FVector4 Color;

    static FPerObjectCBData FromWorldMatrix(const FMatrix& WorldMatrix)
    {
        FPerObjectCBData CBData;
        CBData.Model        = WorldMatrix;
        CBData.NormalMatrix = WorldMatrix.GetInverse().GetTransposed();
        CBData.Color        = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        return CBData;
    }
};

// FFrameCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFrameCBData
{
    FMatrix View;
    FMatrix Projection;
    FMatrix InvViewProj;
    float   bIsWireframe;
    FVector WireframeColor;
    float   Time;
    FVector CameraWorldPos;
};

// FSubUVRegionCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSubUVRegionCBData
{
    float U      = 0.0f;
    float V      = 0.0f;
    float Width  = 1.0f;
    float Height = 1.0f;
};

// FGizmoCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FGizmoCBData
{
    FVector4 ColorTint;
    uint32   bIsInnerGizmo;
    uint32   bClicking;
    uint32   SelectedAxis;
    float    HoveredAxisOpacity;
    uint32   AxisMask;
    uint32   _pad[3];
};

// FOutlinePostProcessCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FOutlinePostProcessCBData
{
    FVector4 OutlineColor     = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
    float    OutlineThickness = 1.0f;
    float    Padding[3]       = {};
};

// FSceneDepthCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSceneDepthCBData
{
    float   Exponent;
    float   NearClip;
    float   FarClip;
    float   Range;
    uint32  Mode;
    FVector _Padding;
};

// FFogCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFogCBData
{
    FVector4 InscatteringColor;
    float    Density;
    float    HeightFalloff;
    float    FogBaseHeight;
    float    StartDistance;
    float    CutoffDistance;
    float    MaxOpacity;
    float    _pad[2];
};

// FFXAACBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFXAACBData
{
    float EdgeThreshold;
    float EdgeThresholdMin;
    float _pad[2];
};

constexpr uint32 MAX_DIRECTIONAL_LIGHTS = 4;

// FAmbientLightCBData는 전역 라이트 상수 버퍼에 기록되는 Ambient 라이트 데이터입니다.
struct FAmbientLightCBData
{
    FVector Color;
    float   Intensity;
};

// FDirectionalLightCBData는 전역 라이트 상수 버퍼에 기록되는 Directional 라이트 데이터입니다.
// Matrix의 SIMD 연산 지원때문에 16bit 대신 32bit 단위 align 필수
struct alignas(32) FDirectionalLightCBData
{
    FVector  Color;          // 12B
    float    Intensity;      // 4B
    FVector  Direction;      // 12B
    int32    ShadowMapIndex; // 4B
    FMatrix  ShadowViewProj; // 64B
}; // Total: 96B

// FGlobalLightCBData는 전역 라이트 상수 버퍼 레이아웃입니다.
// Matrix의 SIMD 연산 지원때문에 16bit 대신 32bit 단위 align 필수
struct alignas(32) FGlobalLightCBData
{
    FAmbientLightCBData     Ambient;                       // 16B
    float                   _Padding0[2];                  // 16B
    FDirectionalLightCBData Directional[MAX_DIRECTIONAL_LIGHTS]; // 96B * 4 = 384B
    int32                   NumDirectionalLights;          // 4B
    int32                   NumLocalLights;                // 4B
    float                   _Padding1[6];                  // 24B
}; // Total: 448B

// FLocalLightCBData는 로컬 라이트 구조화 버퍼에 기록되는 라이트 데이터입니다.
// Matrix의 SIMD 연산 지원때문에 16bit 대신 32bit 단위 align 필수
struct alignas(32) FLocalLightCBData
{
    FVector  Color;             // 12B
    float    Intensity;         // 4B
    FVector  Position;          // 12B
    float    AttenuationRadius; // 4B
    FVector  Direction;         // 12B
    float    InnerConeAngle;    // 4B
    float    OuterConeAngle;    // 4B
    int32    ShadowMapIndex;    // 4B
    float    _PaddingLocal[2];  // 8B
    FMatrix  ShadowViewProj;    // 64B
}; // Total: 128B

// FLightCullingCBData는 타일 기반 라이트 컬링 상수 버퍼 레이아웃입니다.
struct FLightCullingCBData
{
    uint32 ScreenSizeX;
    uint32 ScreenSizeY;
    uint32 TileSizeX;
    uint32 TileSizeY;

    uint32 Enable25DCulling;
    float  NearZ;
    float  FarZ;
    float  NumLights;
};

// FDecalCBData는 데칼 패스 상수 버퍼 레이아웃입니다.
struct FDecalCBData
{
    FMatrix  WorldToDecal;
    FVector4 Color;
};

// FStaticMeshMaterialViewCBData는 정적 메시 머티리얼 뷰 상수 버퍼 레이아웃입니다.
struct FStaticMeshMaterialViewCBData
{
    FVector4 SectionColor       = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    FVector4 MaterialParam      = FVector4(32.0f, 0.5f, 0.0f, 1.0f);
    uint32   HasBaseTexture     = 0;
    uint32   HasNormalTexture   = 0;
    uint32   HasSpecularTexture = 0;
    float    Padding            = 0.0f;
};

// FHiZParamsCBData는 Hi-Z 생성 compute 셰이더 상수 버퍼 레이아웃입니다.
struct FHiZParamsCBData
{
    uint32 SrcWidth;
    uint32 SrcHeight;
    uint32 _pad[2];
};

// FOcclusionTestParamsCBData는 오클루전 테스트 compute 셰이더 상수 버퍼 레이아웃입니다.
struct FOcclusionTestParamsCBData
{
    FMatrix ViewProj;
    float   ViewportWidth;
    float   ViewportHeight;
    uint32  NumAABBs;
    uint32  HiZMipCount;
};
