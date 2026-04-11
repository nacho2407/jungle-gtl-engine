#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Types/RenderTypes.h"

#include "Render/Pipeline/RenderBus.h"
#include "Render/Pipeline/DrawCommandList.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Helper/LineGeometry.h"
#include "Render/Helper/FontGeometry.h"

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

	const FPassRenderState& GetPassRenderState(ERenderPass Pass) const { return PassRenderStates[(uint32)Pass]; }

private:
	void InitializePassRenderStates();

	void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FFrameContext& Frame);

	// ProxyQueue → FDrawCommand 변환
	void BuildProxyDrawCommands(const FRenderBus& InRenderBus, ID3D11DeviceContext* Ctx);
	void BuildCommandsForProxy(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass,
		const FPassRenderState& PassState, EViewMode ViewMode, ID3D11DeviceContext* Ctx);

	// Dynamic geometry → FDrawCommand 변환 (Font, Line)
	void BuildDynamicDrawCommands(const FFrameContext& Frame, ID3D11DeviceContext* Ctx);

	// PerObjectCB 풀 관리
	void EnsurePerObjectCBPoolCapacity(uint32 RequiredCount);
	FConstantBuffer* GetPerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy);

	// PostProcess Outline — StencilSRV 읽어 edge detection 후 fullscreen draw
	void DrawPostProcessOutline(const FRenderBus& Bus, ID3D11DeviceContext* Context);

private:
	FD3DDevice Device;
	FRenderResources Resources;
	FLineGeometry  EditorLines;
	FLineGeometry  GridLines;
	FFontGeometry  FontGeometry;

	// FDrawCommand 기반 렌더링
	FDrawCommandList DrawCommandList;

	TArray<FConstantBuffer> PerObjectCBPool;

	FPassRenderState PassRenderStates[(uint32)ERenderPass::MAX];
};
