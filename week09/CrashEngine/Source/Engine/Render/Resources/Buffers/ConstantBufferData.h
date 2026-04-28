#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Render/Submission/Atlas/ShadowAtlasTypes.h"

// FPerObjectCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FPerObjectCBData
{
    FMatrix  Model;                   // 64B
    FMatrix  NormalMatrix;            // 64B
    FVector4 Color;                   // 16B
    uint32   DecalIndexOffset = 0;    // 4B
    uint32   DecalCount       = 0;    // 4B
    float    PerObjectPadding[2] = {}; // 8B

    static FPerObjectCBData FromWorldMatrix(const FMatrix& WorldMatrix)
    {
        FPerObjectCBData CBData;
        CBData.Model        = WorldMatrix;
        CBData.NormalMatrix = WorldMatrix.GetInverse().GetTransposed();
        CBData.Color        = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        return CBData;
    }
}; // Total: 160B

// FFrameCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFrameCBData
{
    FMatrix View;               // 64B
    FMatrix Projection;         // 64B
    FMatrix InvViewProj;        // 64B
    float   bIsWireframe;       // 4B
    FVector WireframeColor;     // 12B
    float   Time;               // 4B
    FVector CameraWorldPos;     // 12B
}; // Total: 224B

struct FShadowPassCBData
{
    FMatrix ShadowView;          // 64B
    FMatrix ShadowProjection;    // 64B
    FMatrix ShadowInvViewProj;   // 64B
    float   ShadowNearZ = 0.0f;  // 4B
    float   ShadowFarZ = 1.0f;   // 4B
    uint32  ShadowProjectionType = 0; // 4B, 0: orthographic, 1: perspective
    float   ShadowPassPadding = 0.0f; // 4B
}; // Total: 208B

// FSubUVRegionCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSubUVRegionCBData
{
    float U      = 0.0f; // 4B
    float V      = 0.0f; // 4B
    float Width  = 1.0f; // 4B
    float Height = 1.0f; // 4B
}; // Total: 16B

// FGizmoCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FGizmoCBData
{
    FVector4 ColorTint;          // 16B
    uint32   bIsInnerGizmo;      // 4B
    uint32   bClicking;          // 4B
    uint32   SelectedAxis;       // 4B
    float    HoveredAxisOpacity; // 4B
    uint32   AxisMask;           // 4B
    uint32   _pad[3];            // 12B
}; // Total: 48B

// FOutlinePostProcessCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FOutlinePostProcessCBData
{
    FVector4 OutlineColor     = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // 16B
    float    OutlineThickness = 1.0f;                              // 4B
    float    Padding[3]       = {};                                // 12B
}; // Total: 32B

// FSceneDepthCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSceneDepthCBData
{
    float   Exponent;   // 4B
    float   NearClip;   // 4B
    float   FarClip;    // 4B
    float   Range;      // 4B
    uint32  Mode;       // 4B
    FVector _Padding;   // 12B
}; // Total: 32B

// FFogCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFogCBData
{
    FVector4 InscatteringColor; // 16B
    float    Density;           // 4B
    float    HeightFalloff;     // 4B
    float    FogBaseHeight;     // 4B
    float    StartDistance;     // 4B
    float    CutoffDistance;    // 4B
    float    MaxOpacity;        // 4B
    float    _pad[2];           // 8B
}; // Total: 48B

// FFXAACBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFXAACBData
{
    float EdgeThreshold;    // 4B
    float EdgeThresholdMin; // 4B
    float _pad[2];          // 8B
}; // Total: 16B

// FMomentBlurCBData는 shadow moment blur pass(b2)에 사용하는 상수 버퍼 데이터입니다.
struct FMomentBlurCBData
{
    float TexelSizeX = 0.0f; // 4B
    float TexelSizeY = 0.0f; // 4B
    float Padding0   = 0.0f; // 4B
    float Padding1   = 0.0f; // 4B
}; // Total: 16B

struct alignas(16) FShadowAtlasSampleCBData
{
    int32 PageIndex = -1;                                      // 4B
    int32 SliceIndex = -1;                                     // 4B
    float AtlasTexelSizeX = 1.0f / static_cast<float>(ShadowAtlas::AtlasSize); // 4B
    float AtlasTexelSizeY = 1.0f / static_cast<float>(ShadowAtlas::AtlasSize); // 4B
    float UVScaleX = 1.0f;                                     // 4B
    float UVScaleY = 1.0f;                                     // 4B
    float UVOffsetX = 0.0f;                                    // 4B
    float UVOffsetY = 0.0f;                                    // 4B
}; // Total: 32B

inline FShadowAtlasSampleCBData MakeSampleCBData(const FShadowMapData& Data)
{
    FShadowAtlasSampleCBData Out = {};
    if (!Data.bAllocated)
    {
        Out.PageIndex = -1;
        Out.SliceIndex = -1;
        return Out;
    }

    Out.PageIndex = static_cast<int32>(Data.AtlasPageIndex);
    Out.SliceIndex = static_cast<int32>(Data.SliceIndex);
    Out.UVScaleX = Data.UVScaleOffset.X;
    Out.UVScaleY = Data.UVScaleOffset.Y;
    Out.UVOffsetX = Data.UVScaleOffset.Z;
    Out.UVOffsetY = Data.UVScaleOffset.W;
    return Out;
}

// FShaderMatrix44는 structured buffer stride 불일치를 피하기 위한 shader-friendly 4x4 matrix입니다.
struct alignas(16) FShaderMatrix44
{
    float M[16] = {}; // 64B

    FShaderMatrix44() = default;
    FShaderMatrix44(const FMatrix& InMatrix)
    {
        for (int32 Index = 0; Index < 16; ++Index)
        {
            M[Index] = InMatrix.Data[Index];
        }
    }

    FShaderMatrix44& operator=(const FMatrix& InMatrix)
    {
        for (int32 Index = 0; Index < 16; ++Index)
        {
            M[Index] = InMatrix.Data[Index];
        }
        return *this;
    }
}; // Total: 64B

constexpr uint32 MAX_DIRECTIONAL_LIGHTS = 4;
constexpr uint32 MAX_DIRECTIONAL_SHADOW_CASCADES = ShadowAtlas::MaxCascades;
constexpr uint32 MAX_POINT_SHADOW_FACES = ShadowAtlas::MaxPointFaces;

// FAmbientLightCBData는 전역 라이트 상수 버퍼에 기록되는 Ambient 라이트 데이터입니다.
struct FAmbientLightCBData
{
    FVector Color;   // 12B
    float   Intensity; // 4B
}; // Total: 16B

// FDirectionalLightCBData는 전역 라이트 상수 버퍼에 기록되는 Directional 라이트 데이터입니다.
// Matrix의 SIMD 연산 지원때문에 16bit 대신 32bit 단위 align 필수
struct alignas(32) FDirectionalLightCBData
{
    FVector  Color;                                            // 12B
    float    Intensity;                                        // 4B
    FVector  Direction;                                        // 12B
    int32    CascadeCount = 0;                                 // 4B
    FMatrix  ShadowViewProj[MAX_DIRECTIONAL_SHADOW_CASCADES];  // 256B
    FShadowAtlasSampleCBData ShadowSamples[MAX_DIRECTIONAL_SHADOW_CASCADES]; // 128B
    float    ShadowBias = 0.0f;                                // 4B
    float    ShadowSlopeBias = 0.0f;                           // 4B
    float    ShadowNormalBias = 0.0f;                          // 4B
    float    _Padding0 = 0.0f;                                 // 4B
    FVector4 CascadeSplits = FVector4(0.0f, 0.0f, 0.0f, 0.0f); // 16B
}; // Total: 448B

// FGlobalLightCBData는 전역 라이트 상수 버퍼 레이아웃입니다.
// Matrix의 SIMD 연산 지원때문에 16bit 대신 32bit 단위 align 필수
struct alignas(32) FGlobalLightCBData
{
    FAmbientLightCBData     Ambient;                        // 16B
    int32                   NumDirectionalLights;           // 4B
    int32                   NumLocalLights;                 // 4B
    float                   _Padding0[2];                   // 8B
    FDirectionalLightCBData Directional[MAX_DIRECTIONAL_LIGHTS]; // 1792B
}; // Total: 1824B

// FLocalLightCBData는 로컬 라이트 구조화 버퍼에 기록되는 라이트 데이터입니다.
// Matrix의 SIMD 연산 지원때문에 16bit 대신 32bit 단위 align 필수
struct alignas(16) FLocalLightCBData
{
    uint32   LightType;                                     // 4B
    FVector  Color;                                         // 12B
    float    Intensity;                                     // 4B
    FVector  Position;                                      // 12B
    float    AttenuationRadius;                             // 4B
    FVector  Direction;                                     // 12B
    float    InnerConeAngle;                                // 4B
    float    OuterConeAngle;                                // 4B
    int32    ShadowSampleCount = 0;                         // 4B
    float    _Pad0 = 0.0f;                                  // 4B
    FShaderMatrix44 ShadowViewProj[MAX_POINT_SHADOW_FACES]; // 384B
    FShadowAtlasSampleCBData ShadowSamples[MAX_POINT_SHADOW_FACES]; // 192B
    float    ShadowBias = 0.0f;                             // 4B
    float    ShadowSlopeBias = 0.0f;                        // 4B
    float    ShadowNormalBias = 0.0f;                       // 4B
    float    _Pad1 = 0.0f;                                  // 4B
}; // Total: 656B

// FLightCullingCBData는 타일 기반 라이트 컬링 상수 버퍼 레이아웃입니다.
struct FLightCullingCBData
{
    uint32 ScreenSizeX;      // 4B
    uint32 ScreenSizeY;      // 4B
    uint32 TileSizeX;        // 4B
    uint32 TileSizeY;        // 4B
    uint32 Enable25DCulling; // 4B
    float  NearZ;            // 4B
    float  FarZ;             // 4B
    float  NumLights;        // 4B
}; // Total: 32B

// FDecalCBData는 데칼 패스 상수 버퍼 레이아웃입니다.
struct FDecalCBData
{
    FMatrix  WorldToDecal; // 64B
    FVector4 Color;        // 16B
}; // Total: 80B

// FForwardDecalCBData는 forward opaque pass가 읽는 전역 decal 입력입니다.
struct FForwardDecalCBData
{
    FMatrix  WorldToDecal;     // 64B
    FVector4 Color;            // 16B
    uint32   bEnabled = 0;     // 4B
    float    Padding[3] = {};  // 12B
}; // Total: 96B

struct FForwardDecalGPUData
{
    FMatrix  WorldToDecal;      // 64B
    FVector4 Color;             // 16B
    uint32   TextureIndex = 0;  // 4B
    float    Padding[3] = {};   // 12B
}; // Total: 96B

// FStaticMeshMaterialViewCBData는 정적 메시 머티리얼 뷰 상수 버퍼 레이아웃입니다.
struct FStaticMeshMaterialViewCBData
{
    FVector4 SectionColor       = FVector4(1.0f, 1.0f, 1.0f, 1.0f); // 16B
    FVector4 MaterialParam      = FVector4(32.0f, 0.5f, 0.0f, 1.0f); // 16B
    uint32   HasBaseTexture     = 0;                                  // 4B
    uint32   HasNormalTexture   = 0;                                  // 4B
    uint32   HasSpecularTexture = 0;                                  // 4B
    float    Padding            = 0.0f;                               // 4B
}; // Total: 48B

// FHiZParamsCBData는 Hi-Z 생성 compute 셰이더 상수 버퍼 레이아웃입니다.
struct FHiZParamsCBData
{
    uint32 SrcWidth;  // 4B
    uint32 SrcHeight; // 4B
    uint32 _pad[2];   // 8B
}; // Total: 16B

// FOcclusionTestParamsCBData는 오클루전 테스트 compute 셰이더 상수 버퍼 레이아웃입니다.
struct FOcclusionTestParamsCBData
{
    FMatrix ViewProj;       // 64B
    float   ViewportWidth;  // 4B
    float   ViewportHeight; // 4B
    uint32  NumAABBs;       // 4B
    uint32  HiZMipCount;    // 4B
}; // Total: 80B
