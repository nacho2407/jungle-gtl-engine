#pragma once
#include "Render/Types/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Device/D3DDevice.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"
#include "Render/Types/MaterialTextureSlot.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"

class FShader;

/*
	GPU Constant Buffer 구조체, 섹션별 드로우 정보 등
	렌더링에 필요한 데이터 타입을 정의합니다.
*/

// HLSL CB 바인딩 슬롯 — b0/b1 고정, b2/b3 셰이더별 여분, b4 라이팅
namespace ECBSlot
{
	constexpr uint32 Frame = 0;      // b0: View/Projection/Wireframe (고정)
	constexpr uint32 PerObject = 1;  // b1: Model/Color (고정)
	constexpr uint32 PerShader0 = 2; // b2: 셰이더별 여분 슬롯 #0
	constexpr uint32 PerShader1 = 3; // b3: 셰이더별 여분 슬롯 #1 (PerShader2 예약)
	constexpr uint32 Lighting = 4;   // b4: LightingBuffer (Ambient + Directional + 메타)
	constexpr uint32 Shadow = 5;     // b5: ShadowBuffer (Shadow 행렬 + 파라미터)
}

// HLSL 라이팅 SRV 슬롯 — 프레임에 1회 바인딩 (Forward Shading)
namespace ELightTexSlot
{
	constexpr uint32 AllLights = 8;  // t8:  StructuredBuffer<FLightInfo>
	constexpr uint32 TileLightIndices = 9;  // t9:  StructuredBuffer<uint>
	constexpr uint32 TileLightGrid = 10;  // t10: StructuredBuffer<uint2>
	constexpr uint32 ClusterLightIndexList = 11; // t11 : StructuredBuffer<uint>
	constexpr uint32 ClusterLightGrid = 12; // t12 : StructuredBuffer<uint2>
}

namespace ELightCullingUAVSlot
{
	constexpr uint32 ClusterAABB = 0;
	constexpr uint32 LightIndexList = 1;
	constexpr uint32 LightGrid = 2;
	constexpr uint32 GlobalCount = 3;
}
namespace ELightCullingSRVSlot
{
	constexpr uint32 ClusterAABB = 0;
	constexpr uint32 LightInfos = 1;
}

// HLSL 시스템 텍스처 슬롯 — Renderer가 패스 단위로 바인딩 (프레임 공통)
namespace ESystemTexSlot
{
	constexpr uint32 SceneDepth = 16;          // t16: CopyResource된 Depth (R24_UNORM)
	constexpr uint32 SceneColor = 17;          // t17: CopyResource된 SceneColor (R8G8B8A8_UNORM)
	constexpr uint32 GBufferNormal = 18;       // t18: GBuffer World Normal (R16G16B16A16_FLOAT)
	constexpr uint32 Stencil     = 19;         // t19: CopyResource된 Stencil (X24_G8_UINT)
	constexpr uint32 CullingHeatmap = 20;      // t20: Tile Culling Heatmap (R8G8B8A8_UNORM)
	constexpr uint32 ShadowMapCSM       = 21;  // t21: Directional CSM Texture2DArray (4 cascades)
	constexpr uint32 ShadowMapSpotAtlas = 22;  // t22: Spot Atlas Texture2DArray (multi-page)
	constexpr uint32 ShadowMapPointLightTextureArray = 23;  // t23: Point Light
	constexpr uint32 SpotShadowDatas    = 24;  // t24: StructuredBuffer<FSpotShadowDataGPU>
	constexpr uint32 PointShadowDatas   = 25;  // t25: StructuredBuffer<FPointShadowDataGPU>

	// 하위 호환용 별칭
	constexpr uint32 ShadowMap = ShadowMapCSM;
	constexpr uint32 SpotLightAtlas = ShadowMapSpotAtlas;
}

// HLSL 시스템 샘플러 슬롯 — Renderer가 프레임 시작 시 영구 바인딩
namespace ESamplerSlot
{
	constexpr uint32 LinearClamp = 0; // s0: PostProcess, UI, 기본
	constexpr uint32 LinearWrap = 1; // s1: 메시 텍스처, 데칼
	constexpr uint32 PointClamp = 2;      // s2: 폰트, 깊이/스텐실 정밀 읽기
	constexpr uint32 ShadowComparison = 3; // s3: Shadow PCF (Comparison sampler)
	constexpr uint32 ShadowLinear = 4;     // s4: VSM Shadow (Linear sampler)
}

//PerObject
struct FPerObjectConstants
{
	FMatrix Model;
	FMatrix NormalMatrix;
	FVector4 Color;

	// 기본 PerObject: WorldMatrix + White
	static FPerObjectConstants FromWorldMatrix(const FMatrix& WorldMatrix)
	{
		FPerObjectConstants Result = {};
		Result.Model = WorldMatrix;
		Result.NormalMatrix = WorldMatrix.GetInverse().GetTransposed();
		Result.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		return Result;
	}
};

// =============================================================================
// Shadow 상수
// =============================================================================
static constexpr uint32 MAX_SHADOW_CASCADES      = 4;
static constexpr uint32 MAX_SHADOW_SPOT_LIGHTS   = 64;
static constexpr uint32 MAX_SHADOW_POINT_LIGHTS  = 16;

// =============================================================================
// Per-light Shadow GPU 구조체 — StructuredBuffer용 (t24, t25)
// HLSL ForwardLightData.hlsli 와 1:1 대응
// =============================================================================

// Spot Light: ViewProj + atlas 내 UV rect + page index
struct FSpotShadowDataGPU
{
	FMatrix  ViewProj;           // 64B | offset  0
	FVector4 AtlasScaleBias;     // 16B | offset 64  (xy=scale, zw=bias)
	uint32   PageIndex;          //  4B | offset 80  (Texture2DArray slice)
	float    ShadowBias;         //  4B | offset 84
	float    ShadowSharpen;      //  4B | offset 88
	float    ShadowSlopeBias;    //  4B | offset 92 
	float    ShadowNormalBias;   //  4B | offset 96
	float    SpotPad0[3];		 // 12B | offset 100
};
static_assert(sizeof(FSpotShadowDataGPU) % 16 == 0);

// Point Light: 6면 ViewProj + per-face atlas UV rect
// FMatrix가 __m256 포함 → 32B alignment → 컴파일러가 구조체 끝을 32B 경계로 패딩
struct FPointShadowDataGPU
{
	FMatrix  FaceViewProj[6];          // 384B | offset   0
	FVector4 FaceAtlasScaleBias[6];    //  96B | offset 384  (xy=scale, zw=bias, one per face)
	float    NearZ;                    //   4B | offset 480
	float    FarZ;                     //   4B | offset 484
	float    ShadowBias;               //   4B | offset 488
	float    ShadowSharpen;            //   4B | offset 492
	float    ShadowSlopeBias;          //   4B | offset 496
	float	 ShadowNormalBias;         //   4B
	float    _pad[2];                  //  12B | offset 500  → 합계 512B (32B aligned)
};
static_assert(sizeof(FPointShadowDataGPU) % 16 == 0);
static_assert(sizeof(FPointShadowDataGPU) % 32 == 0, "FPointShadowDataGPU must be 32-byte aligned for FMatrix(__m256)");

// =============================================================================
// Shadow CB (b5) — CSM 행렬 + 공통 파라미터
// HLSL ConstantBuffers.hlsli ShadowBuffer와 1:1 대응
// Per-light 데이터는 StructuredBuffer (t24, t25)로 분리
// =============================================================================
struct FShadowCBData
{
	// Directional CSM
	FMatrix  CSMViewProj[MAX_SHADOW_CASCADES];   // 256B | offset   0
	FVector4 CascadeSplits;                      //  16B | offset 256  (cascade 분할 거리)

	// CSM(Directional) 파라미터 — Spot/Point는 per-light StructuredBuffer(t24,t25) 참조
	float    ShadowBias;                         //   4B | offset 272
	float    ShadowSlopeBias;                    //   4B | offset 276
	float    ShadowNormalBias;					 //   4B
	float    ShadowSharpen;                      //   4B | offset 280

	uint32   ShadowFilterMode;                   //   4B | offset 284  (0=Hard, 1=PCF, 2=VSM)
	uint32   NumCSMCascades;                     //   4B | offset 288
	uint32   NumShadowSpotLights;                //   4B | offset 292
	uint32   NumShadowPointLights;               //   4B | offset 296

	uint32   CSMResolution;                      //   4B | offset 300  → 합계 304B, 16B 정렬 OK
	float    Pad[3];
};
static_assert(sizeof(FShadowCBData) % 16 == 0, "FShadowCBData must be 16-byte aligned");

struct FFrameConstants
{
	FMatrix View;
	FMatrix Projection;
	FMatrix InvProj;
	FMatrix InvViewProj;
	float bIsWireframe;
	FVector WireframeColor;
	float Time;
	FVector CameraWorldPos;
};

// SubUV UV region — atlas frame offset + size (b2 slot, shared with Gizmo)
struct FSubUVRegionConstants
{
	float U = 0.0f;
	float V = 0.0f;
	float Width = 1.0f;
	float Height = 1.0f;
};

struct FGizmoConstants
{
	FVector4 ColorTint;
	uint32 bIsInnerGizmo;
	uint32 bClicking;
	uint32 SelectedAxis;
	float HoveredAxisOpacity;
	uint32 AxisMask;       // 비트 0=X, 1=Y, 2=Z — 1이면 표시, 0이면 숨김. 0x7=전부 표시
	uint32 _pad[3];
};

// PostProcess Outline CB (b3) — HLSL OutlinePostProcessCB와 1:1 대응
struct FOutlinePostProcessConstants
{
	FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
	float OutlineThickness = 1.0f;
	float Padding[3] = {};
};

struct FSceneDepthPConstants
{
	float Exponent;
	float NearClip;
	float FarClip;
	uint32 Mode;
};


// Height Fog CB (b6) — HLSL FogBuffer와 1:1 대응
struct FFogConstants
{
	FVector4 InscatteringColor;  // 16B
	float Density;               // 4B
	float HeightFalloff;         // 4B
	float FogBaseHeight;         // 4B
	float StartDistance;         // 4B  — 16B boundary
	float CutoffDistance;        // 4B
	float MaxOpacity;            // 4B
	float _pad[2];              // 8B  — 16B boundary
};

struct FFXAAConstants
{
	float EdgeThreshold;
	float EdgeThresholdMin;
	float _pad[2];
};

// ============================================================
// 타입별 CB 바인딩 디스크립터 — GPU CB에 업로드할 데이터를 인라인 보관
// ============================================================
struct FConstantBufferBinding
{
	FConstantBuffer* Buffer = nullptr;	// 업데이트할 CB (nullptr이면 미사용)
	uint32 Size = 0;					// 업로드할 바이트 수
	uint32 Slot = 0;					// VS/PS CB 슬롯

	static constexpr size_t kMaxDataSize = 128;
	alignas(16) uint8 Data[kMaxDataSize] = {};

	// Buffer/Size/Slot
	template<typename T>
	T& Bind(FConstantBuffer* InBuffer, uint32 InSlot)
	{
		static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
		Buffer = InBuffer;
		Size = sizeof(T);
		Slot = InSlot;
		return *reinterpret_cast<T*>(Data);
	}

	template<typename T>
	T& As()
	{
		static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
		return *reinterpret_cast<T*>(Data);
	}

	template<typename T>
	const T& As() const
	{
		static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
		return *reinterpret_cast<const T*>(Data);
	}
};

class UMaterial;

// 섹션별 드로우 정보 — 머티리얼 포인터 + 인덱스 범위만 보관
struct FMeshSectionDraw
{
	UMaterial* Material = nullptr;
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
};

