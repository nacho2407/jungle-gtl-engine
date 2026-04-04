#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Types/RenderTypes.h"

#include "Render/Pipeline/RenderBus.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Batcher/LineBatcher.h"
#include "Render/Batcher/FontBatcher.h"
#include "Render/Batcher/SubUVBatcher.h"

#include <functional>

// 패스별 Batcher DrawBatch 바인딩
struct FPassBatcherBinding
{
	std::function<void(ERenderPass, const FRenderBus&, ID3D11DeviceContext*)> DrawBatch;

	explicit operator bool() const { return DrawBatch != nullptr; }
};

// 패스별 기본 렌더 상태 — Single Source of Truth
struct FPassRenderState
{
	EDepthStencilState       DepthStencil   = EDepthStencilState::Default;
	EBlendState              Blend          = EBlendState::Opaque;
	ERasterizerState         Rasterizer     = ERasterizerState::SolidBackCull;
	D3D11_PRIMITIVE_TOPOLOGY Topology       = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bool                     bWireframeAware = false;  // Wireframe 모드 시 래스터라이저 전환
};

class FRenderer
{
public:
	void Create(HWND hWindow);
	void Release();

	void PrepareBatchers(const FRenderBus& InRenderBus);
	void BeginFrame();
	void Render(const FRenderBus& InRenderBus);
	void EndFrame();

	FD3DDevice& GetFD3DDevice() { return Device; }
	FRenderResources& GetResources() { return Resources; }

private:
	void InitializePassRenderStates();
	void InitializePassBatchers();

	void ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode ViewMode);
	void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);

	// 프록시 패스 실행기 — FPrimitiveSceneProxy* 순회, 필드 직접 접근
	void ExecutePass(const TArray<const FPrimitiveSceneProxy*>& Proxies, ID3D11DeviceContext* Context);

	// ExecutePass 내부 헬퍼
	struct FDrawState
	{
		FShader*     LastShader     = nullptr;
		FMeshBuffer* LastMeshBuffer = nullptr;
		ID3D11ShaderResourceView* LastSRV = reinterpret_cast<ID3D11ShaderResourceView*>(~0ull);
		int32        LastUVScroll   = -1;
		bool         bSamplerBound  = false;
		bool         bPerObjectBound = false;

		bool HasBoundSRV() const { return LastSRV != reinterpret_cast<ID3D11ShaderResourceView*>(~0ull); }
	};

	void SortProxies(const TArray<const FPrimitiveSceneProxy*>& Proxies);
	void BindShader(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx, FDrawState& State);
	void BindPerObjectSlot(ID3D11DeviceContext* Ctx, FDrawState& State);
	void BindExtraCB(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx);
	bool BindMeshBuffer(FMeshBuffer* Buffer, ID3D11DeviceContext* Ctx, FDrawState& State);
	void DrawSections(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx, FDrawState& State);
	void DrawSimple(const FPrimitiveSceneProxy& Proxy, ID3D11DeviceContext* Ctx, FDrawState& State);
	void CleanupSRV(ID3D11DeviceContext* Ctx, const FDrawState& State);

	// LineBatcher DrawBatch 공통 — EditorShader 바인딩 + DrawBatch
	void DrawLineBatcher(FLineBatcher& Batcher, ID3D11DeviceContext* Context);

	// PostProcess Outline — StencilSRV 읽어 edge detection 후 fullscreen draw
	void DrawPostProcessOutline(const FRenderBus& Bus, ID3D11DeviceContext* Context);

private:
	FD3DDevice Device;
	FRenderResources Resources;
	FLineBatcher   EditorLineBatcher;
	FLineBatcher   GridLineBatcher;
	FFontBatcher   FontBatcher;
	FSubUVBatcher  SubUVBatcher;

	// 정렬용 멤버 버퍼 (재할당 방지)
	TArray<const FPrimitiveSceneProxy*> SortedProxyBuffer;
	TArray<FSubUVEntry> SortedSubUVBuffer;

	FPassRenderState    PassRenderStates[(uint32)ERenderPass::MAX];
	FPassBatcherBinding PassBatchers[(uint32)ERenderPass::MAX];
};
