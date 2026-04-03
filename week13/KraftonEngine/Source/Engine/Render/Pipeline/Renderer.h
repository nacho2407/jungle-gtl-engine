#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Types/RenderTypes.h"

#include "Render/Pipeline/RenderBus.h"
#include "Render/Pipeline/PrimitiveSceneProxy.h"
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

	// 통합 패스 실행기 — FRenderCommand 또는 FPrimitiveSceneProxy* 순회
	// 두 타입이 동일 필드명(Shader, MeshBuffer, PerObjectConstants, SectionDraws, ExtraCB)을 공유
	template<typename TItem>
	void ExecutePass(const TArray<TItem>& Items, ID3D11DeviceContext* Context);

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

	// SubUV 정렬용 멤버 버퍼 (재할당 방지)
	TArray<FSubUVEntry> SortedSubUVBuffer;

	FPassRenderState    PassRenderStates[(uint32)ERenderPass::MAX];
	FPassBatcherBinding PassBatchers[(uint32)ERenderPass::MAX];
};
