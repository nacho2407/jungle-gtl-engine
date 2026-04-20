#ifndef FORWARD_LIGHT_DATA_HLSLI
#define FORWARD_LIGHT_DATA_HLSLI

// =============================================================================
// Forward Shading 라이팅 구조체 & 리소스 바인딩
// C++ ForwardLightData.h 와 바이트 단위로 1:1 대응
//
// 슬롯 배치:
//   b4        LightingBuffer (Ambient + Directional + 메타)
//   t8        StructuredBuffer<FLightInfo>  (Point/Spot 통합)
//   t9        StructuredBuffer<uint>        (TileLightIndices)
//   t10       StructuredBuffer<uint2>       (TileLightGrid)
// =============================================================================

// ── Light Type 상수 ──
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT  1

// ── Tile Culling 상수 ──
#define TILE_SIZE             16
#define MAX_LIGHTS_PER_TILE   256

// =============================================================================
// 구조체 — C++ POD와 레이아웃 동일
// =============================================================================
struct FAABB
{
    float4 minPt;
    float4 maxPt;
};
struct FAmbientLightInfo
{
    float4 Color; // 16B
    float Intensity; //  4B
    float3 _padA; // 12B → 32B
};

struct FDirectionalLightInfo
{
    float4 Color; // 16B
    float3 Direction; // 12B
    float Intensity; //  4B → 32B
};

// Point/Spot 통합 POD — LightType으로 분기
struct FLightInfo
{
    float4 Color; // 16B

    float3 Position; // 12B
    float Intensity; //  4B

    float AttenuationRadius; //  4B
    float FalloffExponent; //  4B
    uint LightType; //  4B
    float _pad0; //  4B

    float3 Direction; // 12B  (Spot 전용)
    float InnerConeCos; //  4B  (Spot 전용)

    float OuterConeCos; //  4B  (Spot 전용)
    float3 _pad1; // 12B → 합계 80B
};
struct FClusterCullingState
{
    float NearZ;
    float FarZ;
    uint ClusterX;
    uint ClusterY;

    uint ClusterZ;
    uint ScreenWidth;
    uint ScreenHeight;
    uint MaxLightsPerCluster;
};

// =============================================================================
// 리소스 바인딩
// =============================================================================

// ── Lighting CB (b3) — Ambient + Directional + 메타데이터 ──
cbuffer LightingBuffer : register(b4)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;

    uint NumActivePointLights;
    uint NumActiveSpotLights;
    uint NumTilesX;
    uint NumTilesY;
    FClusterCullingState CullState;

    uint  ShowClusterHeatMap;  //  4B | offset 112
    float MaxHeatLightCount;   //  4B | offset 116
    uint2 _padFlags;           //  8B | offset 120 → 128B
};

// ── Structured Buffers (t8~t10) ──
StructuredBuffer<FLightInfo> g_AllLights : register(t8);
StructuredBuffer<uint> g_TileLightIndices : register(t9);
StructuredBuffer<uint2> g_TileLightGrid : register(t10);
StructuredBuffer<uint> g_ClusterLightIndices : register(t11);
StructuredBuffer<uint2> g_ClusterLightGrid : register(t12);

#endif // FORWARD_LIGHT_DATA_HLSLI
