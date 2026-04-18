#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Types/RenderTypes.h"

#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/DrawCommandBuilder.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"

class FScene;

// 패스별 기본 렌더 상태 — Single Source of Truth
struct FPassRenderState
{
	EDepthStencilState       DepthStencil = EDepthStencilState::Default;
	EBlendState              Blend = EBlendState::Opaque;
	ERasterizerState         Rasterizer = ERasterizerState::SolidBackCull;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bool                     bWireframeAware = false;  // Wireframe 모드 시 래스터라이저 전환
};

class FRenderer
{
public:
	void Create(HWND hWindow);
	void Release();

	// --- Render phase: 정렬 + GPU 제출 ---
	void BeginFrame();
	void Render(const FFrameContext& Frame, FScene& Scene);
	void EndFrame();

	FD3DDevice& GetFD3DDevice() { return Device; }
	FRenderResources& GetResources() { return Resources; }

	// Collect 페이즈에서 커맨드 빌드를 담당하는 Builder
	FDrawCommandBuilder& GetBuilder() { return Builder; }

private:
	void InitializePassRenderStates();

	void UpdateFrameBuffer(const FFrameContext& Frame);
	void UpdateLightBuffer(const FScene& Scene);

	// 패스 루프 Pre/Post 이벤트 등록
	void BuildPassEvents(TArray<struct FPassEvent>& PrePassEvents,
		TArray<struct FPassEvent>& PostPassEvents,
		const FFrameContext& Frame, FStateCache& Cache);

	// 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
	void CleanupPassState(FStateCache& Cache);

private:
	FD3DDevice Device;
	FRenderResources Resources;

	FDrawCommandBuilder Builder;

	FPassRenderState PassRenderStates[(uint32)ERenderPass::MAX];
};
