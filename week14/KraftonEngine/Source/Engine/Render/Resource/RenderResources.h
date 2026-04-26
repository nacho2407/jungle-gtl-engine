#pragma once
#include "Render/Resource/Buffer.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Types/ForwardLightData.h"

#include "Render/RenderState/RasterizerStateManager.h"
#include "Render/RenderState/DepthStencilStateManager.h"
#include "Render/RenderState/BlendStateManager.h"
#include "Render/RenderState/SamplerStateManager.h"
#include "Render/Culling/TileBasedLightCulling.h"
#include "Render/Culling/ClusteredLightCuller.h"

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
	ID3D11Texture2D*          CSMTexture  = nullptr;
	ID3D11DepthStencilView*   CSMDSV[MAX_SHADOW_CASCADES] = {};
	ID3D11ShaderResourceView* CSMSRV      = nullptr;              // 전체 array SRV (셰이더용)
	ID3D11ShaderResourceView* CSMSliceSRV[MAX_SHADOW_CASCADES] = {}; // per-cascade SRV (ImGui 디버그용)
	uint32 CSMResolution = 2048;
	FVector4 CSMDebugCascadeNear = {};
	FVector4 CSMDebugCascadeFar = {};

	// ── Spot Light Atlas (t22) ──
	ID3D11Texture2D*           SpotAtlasTexture   = nullptr;
	ID3D11DepthStencilView**   SpotAtlasDSVs      = nullptr;
	ID3D11ShaderResourceView*  SpotAtlasSRV       = nullptr;
	ID3D11ShaderResourceView** SpotAtlasSliceSRVs = nullptr;  // per-slice SRV (ImGui 디버그용)
	uint32 SpotAtlasResolution = 4096;
	uint32 SpotAtlasPageCount  = 0;

	// ── Point Light CubeMap (t23) ──
	ID3D11Texture2D*          PointCubeTexture = nullptr;
	ID3D11DepthStencilView**  PointCubeDSVs    = nullptr;
	ID3D11ShaderResourceView* PointCubeSRV     = nullptr;
	uint32 PointCubeResolution = 1024;
	uint32 PointCubeCount      = 0;

	// ── Per-light StructuredBuffers (t24, t25) ──
	ID3D11Buffer*             SpotShadowDataBuffer  = nullptr;
	ID3D11ShaderResourceView* SpotShadowDataSRV     = nullptr;
	uint32                    SpotShadowDataCapacity = 0;

	ID3D11Buffer*             PointShadowDataBuffer = nullptr;
	ID3D11ShaderResourceView* PointShadowDataSRV    = nullptr;
	uint32                    PointShadowDataCapacity = 0;

	bool IsCSMValid()   const { return CSMTexture != nullptr; }
	bool IsSpotValid()  const { return SpotAtlasTexture != nullptr && SpotAtlasPageCount > 0; }
	bool IsPointValid() const { return PointCubeTexture != nullptr && PointCubeCount > 0; }

	void EnsureCSM(ID3D11Device* Device, uint32 Resolution);
	void EnsureSpotAtlas(ID3D11Device* Device, uint32 Resolution, uint32 PageCount);
	void EnsurePointCube(ID3D11Device* Device, uint32 Resolution, uint32 CubeCount);
	void Release();
};

/*
	시스템 레벨 GPU 리소스를 관리하는 구조체입니다.
	프레임 공용 CB (Frame, Lighting), 라이트 StructuredBuffer,
	렌더 상태 오브젝트(DSS/Blend/Rasterizer/Sampler),
	시스템 텍스처 언바인딩(t16-t19), Shadow 텍스처(t21-t25)를 소유합니다.
	셰이더별 CB(Gizmo, PostProcess 등)는 각 소유자(Proxy, Builder)가 직접 관리합니다.
*/

class FD3DDevice;
class FScene;
struct FFrameContext;

struct FLightingResource
{
	ID3D11Buffer* LightBuffer = nullptr;
	ID3D11ShaderResourceView* LightBufferSRV = nullptr;
	uint32 MaxLightCount = 0;

	void Create(ID3D11Device* InDevice, uint32 MaxLightCount)
	{
		this->MaxLightCount = MaxLightCount;
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = sizeof(FLightInfo) * this->MaxLightCount;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.StructureByteStride = sizeof(FLightInfo);
		InDevice->CreateBuffer(&Desc, nullptr, &LightBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.NumElements = this->MaxLightCount;
		InDevice->CreateShaderResourceView(LightBuffer, &SRVDesc, &LightBufferSRV);
	}

	void Update(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, const TArray<FLightInfo>& LightInfos)
	{
		if (MaxLightCount < LightInfos.size())
		{
			Release();
			uint32 NewCount = MaxLightCount;
			while (NewCount < LightInfos.size())
				NewCount *= 2;
			Create(InDevice, NewCount);
		}

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		InDeviceContext->Map(LightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
		memcpy(Mapped.pData, LightInfos.data(), sizeof(FLightInfo) * LightInfos.size());
		InDeviceContext->Unmap(LightBuffer, 0);
	}

	void Release()
	{
		if (LightBuffer) { LightBuffer->Release(); LightBuffer = nullptr; }
		if (LightBufferSRV) { LightBufferSRV->Release(); LightBufferSRV = nullptr; }
	}
};

struct FTileCullingResource
{
	// Buffers
	ID3D11Buffer* IndicesBuffer  = nullptr;
	ID3D11Buffer* GridBuffer     = nullptr;
	ID3D11Buffer* CounterBuffer  = nullptr;

	// UAVs — CS writes (u0, u1, u2)
	ID3D11UnorderedAccessView* IndicesUAV = nullptr;
	ID3D11UnorderedAccessView* GridUAV    = nullptr;
	ID3D11UnorderedAccessView* CounterUAV = nullptr;

	// SRVs — PS reads (t9, t10)
	ID3D11ShaderResourceView*  IndicesSRV = nullptr;
	ID3D11ShaderResourceView*  GridSRV    = nullptr;

	uint32 TileCountX = 0;
	uint32 TileCountY = 0;

	void Create(ID3D11Device* Dev, uint32 InTileCountX, uint32 InTileCountY);
	void Release();
};

struct FSystemResources
{
	// --- Frame CB (b0) ---
	FConstantBuffer FrameBuffer;				// b0 — ECBSlot::Frame

	// --- Lighting ---
	FConstantBuffer LightingConstantBuffer;		// b4 — ECBSlot::Lighting
	FLightingResource ForwardLights;			// t8 — ELightTexSlot::AllLights
	FTileCullingResource TileCullingResource;	// t9/t10 — 타일 컬링 결과 버퍼
	uint32 LastNumLights = 0;					// Dispatch용 총 라이트 수 캐시

	// --- Light Culling ---
	FTileBasedLightCulling TileBasedCulling;
	FClusteredLightCuller  ClusteredLightCuller;

	// --- Shadow ---
	FShadowMapResources ShadowResources;			// t21-t25 텍스처/SRV/StructuredBuffer
	FConstantBuffer ShadowConstantBuffer;			// b5 — ECBSlot::Shadow

	// --- Render State Managers ---
	FRasterizerStateManager RasterizerStateManager;
	FDepthStencilStateManager DepthStencilStateManager;
	FBlendStateManager BlendStateManager;
	FSamplerStateManager SamplerStateManager;		// s0-s2

	void Create(ID3D11Device* InDevice);
	void Release();

	// 렌더 상태 전환
	void SetDepthStencilState(FD3DDevice& Device, EDepthStencilState InState);
	void SetBlendState(FD3DDevice& Device, EBlendState InState);
	void SetRasterizerState(FD3DDevice& Device, ERasterizerState InState);

	// 리사이즈 시 렌더 상태 캐시 무효화
	void ResetRenderStateCache();

	// 프레임 공용 CB 업데이트 + 바인딩 (b0)
	void UpdateFrameBuffer(FD3DDevice& Device, const FFrameContext& Frame);

	// 라이팅 CB + StructuredBuffer 업데이트 + 바인딩 (b4, t8)
	void UpdateLightBuffer(FD3DDevice& Device, const FScene& Scene, const FFrameContext& Frame);

	// s0-s2 시스템 샘플러 일괄 바인딩 (프레임 1회)
	void BindSystemSamplers(FD3DDevice& Device);

	// 타일 컬링 결과 SRV 바인딩 (t9, t10) — Renderer::Render 시작 시 호출
	void BindTileCullingBuffers(FD3DDevice& Device);
	void UnbindTileCullingBuffers(FD3DDevice& Device);

	// 시스템 텍스처 슬롯 언바인딩 (t16-t19)
	void UnbindSystemTextures(FD3DDevice& Device);

	// --- Light Culling Dispatch/Bind ---
	void DispatchTileCulling(ID3D11DeviceContext* DC, const FFrameContext& Frame);
	void DispatchClusterCulling(FD3DDevice& Device);
	void BindClusterCullingResources(FD3DDevice& Device);
	void UnbindClusterCullingResources(FD3DDevice& Device);
	void SubmitCullingDebugLines(ID3D11DeviceContext* DC, class UWorld* World);
};
