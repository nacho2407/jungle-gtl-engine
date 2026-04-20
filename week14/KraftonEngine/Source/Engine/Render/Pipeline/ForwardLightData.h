#pragma once
#include "Math/Vector.h"
#include "Core/EngineTypes.h"

/*
	Forward Shading 라이팅 GPU 구조체
	HLSL Common/ForwardLightData.hlsli 와 1:1 대응

	슬롯 배치:
	  b4        LightingBuffer (Ambient + Directional, 프레임 고정)
	  t8        StructuredBuffer<FLightInfo>  (Point/Spot 통합 POD)
	  t9        StructuredBuffer<uint>        (TileLightIndices)
	  t10       StructuredBuffer<uint2>       (TileLightGrid)
*/

// =============================================================================
// Light Type 상수 — HLSL LIGHT_TYPE_* 과 1:1 대응
// =============================================================================
namespace ELightType
{
	constexpr uint32 Point = 0;
	constexpr uint32 Spot = 1;
}

// =============================================================================
// GPU POD 구조체 — 16바이트 정렬, HLSL과 바이트 단위로 일치해야 함
// =============================================================================

// Ambient Light — CB 직행 (Tile Culling 대상 아님)
struct FAmbientLightGPU
{
	FVector4 Color;       // 16B
	float    Intensity;   //  4B
	float    _pad[3];     // 12B → 합계 32B (16B 정렬)
};

// Directional Light — CB 직행 (Tile Culling 대상 아님)
struct FDirectionalLightGPU
{
	FVector4 Color;       // 16B
	FVector  Direction;   // 12B
	float    Intensity;   //  4B → 합계 32B (16B 정렬)
};

// Point/Spot 통합 POD — StructuredBuffer<FLightInfo> (t8)
// GPU는 LightType으로 분기, CPU는 다형성(ToGPULightInfo)으로 채움
struct FLightInfo
{
	// ── 공통 (Point + Spot) ──
	FVector4 Color;                   // 16B  | offset  0

	FVector  Position;                // 12B  | offset 16
	float    Intensity;               //  4B  | offset 28

	float    AttenuationRadius;       //  4B  | offset 32
	float    FalloffExponent;         //  4B  | offset 36
	uint32   LightType;              //  4B  | offset 40  (ELightType::Point or Spot)
	float    _pad0;                   //  4B  | offset 44

	// ── Spot 전용 (Point일 때 무시됨) ──
	FVector  Direction;               // 12B  | offset 48
	float    InnerConeCos;            //  4B  | offset 60  (cos(innerAngle), C++에서 미리 계산)

	float    OuterConeCos;            //  4B  | offset 64  (cos(outerAngle))
	float    _pad1[3];               // 12B  | offset 68  → 합계 80B (16B 정렬)
};
static_assert(sizeof(FLightInfo) % 16 == 0, "FLightInfo must be 16-byte aligned for StructuredBuffer");
static_assert(sizeof(FLightInfo) == 80, "FLightInfo size mismatch with HLSL");

// =============================================================================
// Lighting Constant Buffer (b3) — Ambient + Directional + 메타데이터
// =============================================================================

struct FClusterCullingState
{
	float NearZ;
	float FarZ;
	uint32 ClusterX = 32;
	uint32 ClusterY = 18;

	uint32 ClusterZ = 32;
	uint32 ScreenWidth = 0;
	uint32 ScreenHeight = 0;
	uint32 MaxLightsPerCluster = 128;
};

struct FLightingCBData
{
	FAmbientLightGPU     Ambient;              // 32B  | offset  0
	FDirectionalLightGPU Directional;          // 32B  | offset 32

	uint32  NumActivePointLights;              //  4B  | offset 64
	uint32  NumActiveSpotLights;               //  4B  | offset 68
	uint32  NumTilesX;                         //  4B  | offset 72  (Tile Culling용)
	uint32  NumTilesY;                         //  4B  | offset 76  → 합계 80B (16B 정렬)
	FClusterCullingState ClusterCullingState;  // 32B  | offset 80

	uint32  ShowClusterHeatMap;                //  4B  | offset 112
	float   MaxHeatLightCount;                 //  4B  | offset 116
	uint32  _padFlags[2];                      //  8B  | offset 120 → 합계 128B
};
static_assert(sizeof(FLightingCBData) % 16 == 0, "FLightingCBData must be 16-byte aligned");
static_assert(sizeof(FLightingCBData) == 128, "FLightingCBData size mismatch with HLSL");

// =============================================================================
// Tile-based Light Culling 상수
// =============================================================================
namespace ETileCulling
{
	constexpr uint32 TileSize = 16;   // 16x16 픽셀
	constexpr uint32 MaxLightsPerTile = 256;
	constexpr uint32 MaxLights = 1024; // g_AllLights 최대 크기
}
