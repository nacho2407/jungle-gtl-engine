#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Common/RenderTypes.h"

#include "Render/Scene/RenderBus.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/LineBatcher.h"
#include "Render/FontBatcher.h"
#include "Render/SubUVBatcher.h"

#include <cstddef>
#include <functional>

// 패스별 Batcher 바인딩 — Clear → Collect → Flush 패턴
struct FPassBatcherBinding
{
	std::function<void()> Clear;
	std::function<void(const FRenderCommand&, const FRenderBus&)> Collect;
	std::function<void(ERenderPass, const FRenderBus&, ID3D11DeviceContext*)> Flush;

	explicit operator bool() const { return Flush != nullptr; }
};

// 패스별 기본 렌더 상태 — Single Source of Truth
struct FPassRenderState
{
	EDepthStencilState       DepthStencil   = EDepthStencilState::Default;
	EBlendState              Blend          = EBlendState::Opaque;
	ERasterizerState         Rasterizer     = ERasterizerState::SolidBackCull;
	D3D11_PRIMITIVE_TOPOLOGY Topology       = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FShader*                 Shader         = nullptr; // nullptr → batcher가 자체 셰이더 사용
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
	void BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context);

	void DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand);
	void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);

	// 기본 패스 실행기 — SetupRenderState + DrawCommand 루프
	void ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus, ID3D11DeviceContext* Context);

	// LineBatcher Flush 공통 — EditorConstants 업데이트 + EditorShader 바인딩
	void FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Context);

private:
	FD3DDevice Device;
	FRenderResources Resources;
	FLineBatcher   EditorLineBatcher;
	FLineBatcher   GridLineBatcher;
	FFontBatcher   FontBatcher;
	FSubUVBatcher  SubUVBatcher;

	FPassRenderState    PassRenderStates[(uint32)ERenderPass::MAX];
	FPassBatcherBinding PassBatchers[(uint32)ERenderPass::MAX];
	ID3D11ShaderResourceView* SubUVCachedSRV = nullptr;

	//	Primitive and Gizmo Input Layout
	D3D11_INPUT_ELEMENT_DESC PrimitiveInputLayout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  static_cast<uint32>(offsetof(FVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

};

