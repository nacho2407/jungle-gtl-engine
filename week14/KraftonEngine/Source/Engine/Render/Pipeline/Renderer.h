#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Types/RenderTypes.h"

#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/DrawCommandList.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Helper/LineGeometry.h"
#include "Render/Helper/FontGeometry.h"
#include "Render/Culling/ClusteredLightCuller.h"

class FTextRenderSceneProxy;
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

	// --- Collect phase: Pipeline이 호출하여 커맨드 수집 시작/종료 ---
	// MaxProxyCount: Scene의 프록시 수. PerObjectCBPool을 미리 할당하여
	// Collect 도중 resize로 인한 포인터 무효화를 방지.
	void BeginCollect(const FFrameContext& Frame, uint32 MaxProxyCount = 0);

	// Collector가 직접 호출 — Proxy → FDrawCommand 변환
	void BuildCommandForProxy(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass);
	void BuildDecalCommandForReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy);

	// Collector가 직접 호출 — Font proxy → FontGeometry 배칭
	void AddWorldText(const FTextRenderSceneProxy* TextProxy, const FFrameContext& Frame);

	// Collect 마무리: FScene 경량 데이터(DebugLine, Grid, OverlayText) →
	// 동적 지오메트리 → FDrawCommand 변환. Pipeline의 Collect 블록 끝에서 호출.
	void BuildDynamicCommands(const FFrameContext& Frame, const FScene* Scene);

	// --- Render phase: 정렬 + GPU 제출 ---
	void BeginFrame();
	void Render(const FFrameContext& Frame, FScene& Scene);
	void EndFrame();

	FD3DDevice& GetFD3DDevice() { return Device; }
	FRenderResources& GetResources() { return Resources; }

	const FPassRenderState& GetPassRenderState(ERenderPass Pass) const { return PassRenderStates[(uint32)Pass]; }


private:
	void InitializePassRenderStates();

	void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FFrameContext& Frame);
	void UpdateLightBuffer(ID3D11Device* InDevice, ID3D11DeviceContext* Context, const FScene& Scene, const FFrameContext& Frame);
	void UpdateLightCullingSRVs(ID3D11DeviceContext* Context);
	void UnbindLightCullingSRVs(ID3D11DeviceContext* Context);
	//void UpdateLightBuffer(ID3D11DeviceContext* Context, const )

	// 동적 지오메트리 (DebugLine, Grid, OverlayText) → 라인/폰트 헬퍼
	void PrepareDynamicGeometry(const FFrameContext& Frame, const FScene* Scene);

	// 동적 지오메트리 + PostProcess → FDrawCommand (VB 업로드 + 커맨드 생성)
	void BuildDynamicDrawCommands(const FFrameContext& Frame, ID3D11DeviceContext* Ctx, const FScene* Scene);

	// 패스 루프 Pre/Post 이벤트 등록
	void BuildPassEvents(TArray<struct FPassEvent>& PrePassEvents,
		TArray<struct FPassEvent>& PostPassEvents,
		ID3D11DeviceContext* Context, const FFrameContext& Frame, FStateCache& Cache);

	// 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
	void CleanupPassState(ID3D11DeviceContext* Context, FStateCache& Cache);

	// PerObjectCB 풀 관리
	void EnsurePerObjectCBPoolCapacity(uint32 RequiredCount);
	FConstantBuffer* GetPerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy);

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

	// BeginCollect에서 저장, BuildCommandForProxy에서 사용
	EViewMode CollectViewMode = EViewMode::Lit_Phong;
	bool bHasSelectionMaskCommands = false;



	//LightCulling
	FClusteredLightCuller ClusteredLightCuller;
};
