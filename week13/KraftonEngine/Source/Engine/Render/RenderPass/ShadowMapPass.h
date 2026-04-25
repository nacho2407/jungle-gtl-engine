#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/RenderConstants.h"
#include "Math/Matrix.h"

#include "Render/Shadow/ShadowAtlasQuadTree.h"
#include <d3d11.h>

/*
	FShadowMapResources — Shadow 텍스처 GPU 리소스 통합 관리.

	t21: Directional CSM     — Texture2DArray (MAX_SHADOW_CASCADES slices)
	t22: Spot Light Atlas    — Texture2DArray (page 단위, 동적 slice 수)
	t23: Point Light CubeMap — TextureCubeArray (동적 cube 수)
	t24: StructuredBuffer<FSpotShadowDataGPU>  (per-spot 행렬 + atlas rect)
	t25: StructuredBuffer<FPointShadowDataGPU> (per-point 6면 행렬)
*/
struct FShadowMapResources
{
	// ── Directional CSM (t21) ──
	// Texture2DArray: ArraySize = MAX_SHADOW_CASCADES (4)
	ID3D11Texture2D*          CSMTexture  = nullptr;
	ID3D11DepthStencilView*   CSMDSV[MAX_SHADOW_CASCADES] = {};  // per-cascade DSV
	ID3D11ShaderResourceView* CSMSRV      = nullptr;              // 전체 array SRV
	uint32 CSMResolution = 2048;

	// ── Spot Light Atlas (t22) ──
	// Texture2DArray: ArraySize = NumPages (동적)
	ID3D11Texture2D*          SpotAtlasTexture = nullptr;
	ID3D11DepthStencilView**  SpotAtlasDSVs    = nullptr;  // per-page DSV (동적 배열)
	ID3D11ShaderResourceView* SpotAtlasSRV     = nullptr;
	uint32 SpotAtlasResolution = 4096;  // per-page 해상도
	uint32 SpotAtlasPageCount  = 0;     // 현재 할당된 page 수

	// ── Point Light CubeMap (t23) ──
	// TextureCubeArray: ArraySize = NumCubes * 6
	ID3D11Texture2D*          PointCubeTexture = nullptr;
	ID3D11DepthStencilView**  PointCubeDSVs    = nullptr;  // per-face DSV (NumCubes*6, 동적 배열)
	ID3D11ShaderResourceView* PointCubeSRV     = nullptr;
	uint32 PointCubeResolution = 1024;
	uint32 PointCubeCount      = 0;     // 현재 할당된 cube 수

	// ── Per-light StructuredBuffers (t24, t25) ──
	ID3D11Buffer*             SpotShadowDataBuffer  = nullptr;  // StructuredBuffer<FSpotShadowDataGPU>
	ID3D11ShaderResourceView* SpotShadowDataSRV     = nullptr;
	uint32                    SpotShadowDataCapacity = 0;

	ID3D11Buffer*             PointShadowDataBuffer = nullptr;  // StructuredBuffer<FPointShadowDataGPU>
	ID3D11ShaderResourceView* PointShadowDataSRV    = nullptr;
	uint32                    PointShadowDataCapacity = 0;

	bool IsCSMValid()   const { return CSMTexture != nullptr; }
	bool IsSpotValid()  const { return SpotAtlasTexture != nullptr && SpotAtlasPageCount > 0; }
	bool IsPointValid() const { return PointCubeTexture != nullptr && PointCubeCount > 0; }
};

/*
	FShadowMapPass — 라이트별 Shadow Depth 렌더링 패스.
	LightCulling(1)과 Opaque(2) 사이에 실행되며,
	각 라이트의 ViewProj로 depth-only 렌더링을 수행합니다.

	리소스 관리:
	  - CSM: 프레임마다 고정 4 cascade (해상도 변경 시 재생성)
	  - Spot Atlas: 라이트 수 변동 시 page 추가/축소
	  - Point Cube: 라이트 수 변동 시 cube 추가/축소
*/
class FShadowMapPass final : public FRenderPassBase
{
public:
	FShadowMapPass();
	~FShadowMapPass();

	void BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;

	// Shadow 리소스 접근 — Renderer에서 Opaque 패스에 바인딩할 때 사용
	const FShadowMapResources& GetResources() const { return Resources; }

	// 유효한 shadow가 있는지
	bool HasValidShadow() const { return bHasValidShadow; }

	// Legacy 호환 — 단일 SRV/ViewProj (CSM cascade 0 기준)
	ID3D11ShaderResourceView* GetShadowMapSRV() const { return Resources.CSMSRV; }
	const FMatrix& GetLightViewProj() const { return LightViewProj; }

private:
	void EnsureCSM(ID3D11Device* Device, uint32 Resolution);
	void EnsureSpotAtlas(ID3D11Device* Device, uint32 Resolution, uint32 PageCount);
	void EnsurePointCube(ID3D11Device* Device, uint32 Resolution, uint32 CubeCount);
	void ReleaseResources();

	void PreSpotlightAtlas();
	void DrawSpotLightAtlas();

private:
	FShadowMapResources Resources;

	// Shadow 렌더링용 PerObject CB (b1)
	FConstantBuffer ShadowPerObjectCB;

	// Legacy — 이번 프레임의 Light ViewProj
	FMatrix LightViewProj;
	bool bHasValidShadow = false;

	// Spotlight fields (legacy — FShadowMapResources로 이관 예정)
	ID3D11Texture2D*			SpotLightTexture	= nullptr;
	ID3D11DepthStencilView*		SpotLightDSV		= nullptr;
	ID3D11ShaderResourceView*	SpotLightSRV		= nullptr;
	FShadowAtlasQuadTree SpotLightAtlas; // Spot Light용 Shadow Atlas 관리
};
