#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

// Shadow atlas에서 공통으로 쓰는 상수와 metadata 타입을 모아둔 헤더입니다.
namespace ShadowAtlas
{
constexpr uint32 AtlasSize         = 4096;
constexpr uint32 SliceCount        = 4;
constexpr uint32 MaxPages          = 2;
constexpr uint32 Padding           = 4;
constexpr uint32 MinResolution     = 256;
constexpr uint32 MaxResolution     = 4096;
constexpr uint32 MaxCascades       = 4;
constexpr uint32 MaxPointFaces     = 6;
constexpr uint32 InvalidPageIndex  = 0xFFFFFFFFu;
constexpr uint32 InvalidSliceIndex = 0xFFFFFFFFu;
constexpr uint32 InvalidNodeIndex  = 0xFFFFFFFFu;
} // namespace ShadowAtlas

// Atlas 안에서 실제로 차지하는 사각형 영역입니다.
struct FShadowAtlasRect
{
    uint32 X      = 0;
    uint32 Y      = 0;
    uint32 Width  = 0;
    uint32 Height = 0;
};

// Light 하나의 shadow map이 atlas 어디에 배치됐는지 기록합니다.
struct FShadowMapData
{
    bool             bAllocated = false;
    uint32           AtlasPageIndex = ShadowAtlas::InvalidPageIndex;
    uint32           SliceIndex = ShadowAtlas::InvalidSliceIndex;
    uint32           NodeIndex = ShadowAtlas::InvalidNodeIndex;
    uint32           Resolution = 0;
    FShadowAtlasRect Rect = {};
    FShadowAtlasRect ViewportRect = {};
    FVector4         UVScaleOffset = FVector4(1.0f, 1.0f, 0.0f, 0.0f);

    void Reset()
    {
        *this = {};
    }
};

// One shadow-rendering view derived from a light.
struct FShadowViewData
{
    FMatrix View = FMatrix::Identity;
    FMatrix Projection = FMatrix::Identity;
    FMatrix ViewProj = FMatrix::Identity;
    float   NearZ = 0.0f;
    float   FarZ = 1.0f;
    uint32  ProjectionType = 0; // 0: orthographic, 1: perspective

    void Set(const FMatrix& InView, const FMatrix& InProjection, float InNearZ, float InFarZ, uint32 InProjectionType)
    {
        View = InView;
        Projection = InProjection;
        ViewProj = InView * InProjection;
        NearZ = InNearZ;
        FarZ = InFarZ;
        ProjectionType = InProjectionType;
    }
};

// Directional light는 cascade별 atlas allocation을 따로 가집니다.
struct FCascadeShadowMapData
{
    FShadowMapData Cascades[ShadowAtlas::MaxCascades] = {};
    FMatrix        CascadeViewProj[ShadowAtlas::MaxCascades] = {};
    FShadowViewData CascadeViews[ShadowAtlas::MaxCascades] = {};
    float          CascadeSplits[ShadowAtlas::MaxCascades + 1] = {};
    uint32         CascadeCount = 0;

    void Reset()
    {
        *this = {};
    }
};

// Point light는 6개 cube face를 각각 atlas allocation으로 가집니다.
struct FCubeShadowMapData
{
    FShadowMapData Faces[ShadowAtlas::MaxPointFaces] = {};
    FMatrix        FaceViewProj[ShadowAtlas::MaxPointFaces] = {};
    FShadowViewData FaceViews[ShadowAtlas::MaxPointFaces] = {};

    void Reset()
    {
        *this = {};
    }
};

// Light별 shadow atlas allocation cache가 보관하는 메타데이터입니다.
struct FLightShadowRecord
{
    uint32                RequestedResolution  = 0;
    uint32                Resolution           = 0;
    uint32                CascadeCount         = 0;
    uint32                LightType            = 0;
    uint64                LastTouchedFrame     = 0;
    uint64                LastAllocationFrame  = 0;
    uint64                LastFailedAllocationFrame = 0;
    uint32                FailedAllocationCount = 0;
    bool                  bTouchedThisFrame    = false;
    FCascadeShadowMapData CascadeShadowMapData = {};
    FShadowMapData        SpotShadowMapData    = {};
    FCubeShadowMapData    CubeShadowMapData    = {};
};

struct FShadowAtlasBudgetStats
{
    uint64 TotalAtlasMemoryBytes = 0;
    uint64 ResidentAtlasMemoryBytes = 0;
    uint64 UsedAtlasMemoryBytes = 0;
    uint64 TotalMomentMemoryBytes = 0;
    uint64 ResidentMomentMemoryBytes = 0;
    uint64 UsedMomentMemoryBytes = 0;
    uint32 UsedPageCount = 0;
    uint32 UsedSliceCount = 0;
    uint32 MomentResidentPageCount = 0;
    float  UsedAreaPercent = 0.0f;
    uint32 AllocatedShadowCount = 0;
    uint32 FailedAllocationCount = 0;
    uint32 EvictedShadowCount = 0;
    uint32 DirectionalShadowLightCount = 0;
    uint32 SpotShadowLightCount = 0;
    uint32 PointShadowLightCount = 0;
};
