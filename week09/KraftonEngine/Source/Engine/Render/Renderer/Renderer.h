#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Types/RenderTypes.h"
#include "Render/Core/RenderPipeline.h"

#include "Render/Core/FrameContext.h"
#include "Render/Core/DrawCommandList.h"
#include "Render/Core/PassRenderState.h"
#include "Render/Core/PassEvent.h"
#include "Render/Scene/PrimitiveSceneProxy.h"
#include "Render/RHI/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/Geometry/LineGeometry.h"
#include "Render/Resource/Geometry/FontGeometry.h"
#include "Render/Renderer/RenderPassRegistry.h"
#include "Render/Renderer/RenderPipelineRegistry.h"
#include "Render/Renderer/RenderPipelineRunner.h"

class FTextRenderSceneProxy;
class FScene;
class FViewModePassRegistry;
class FViewModeSurfaceSet;


class FRenderer
{
public:
	void Create(HWND hWindow);
	void Release();

	void SetActiveViewMode(EViewMode InViewMode);
	void ClearActiveViewMode();
	EViewMode GetActiveViewMode() const { return ActiveViewMode; }
	bool HasActiveViewModePassConfig() const { return bHasActiveViewModePassConfig; }
	bool HasActiveViewModeConfig() const { return HasActiveViewModePassConfig(); }

	FViewModeSurfaceSet* AcquireViewModeSurfaceSet(uint32 Width, uint32 Height);
	void ReleaseViewModeSurfaceSet();
	FViewModeSurfaceSet* GetActiveViewModeSurfaceSet() const { return ActiveViewSurfaceSet; }
	const FViewModePassRegistry* GetViewModePassRegistry() const { return ViewModePassRegistry; }

	// --- Collect phase: Pipeline이 호출하여 커맨드 수집 시작/종료 ---
	// MaxProxyCount: Scene의 프록시 수. PerObjectCBPool을 미리 할당하여
	// Collect 도중 resize로 인한 포인터 무효화를 방지.
	void BeginCollect(const FFrameContext& Frame, uint32 MaxProxyCount = 0);

	// Collector가 직접 호출 — Proxy → FDrawCommand 변환
	void BuildCommandForProxy(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass);
	void BuildDecalCommandForReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy);
	void BuildDecalCommand(const FPrimitiveSceneProxy& DecalProxy);

	// Collector가 직접 호출 — Font proxy → FontGeometry 배칭
	void AddWorldText(const FTextRenderSceneProxy* TextProxy, const FFrameContext& Frame);

	// Collect 마무리: FScene 경량 데이터(DebugLine, Grid, OverlayText) →
	// 동적 지오메트리 → FDrawCommand 변환. Pipeline의 Collect 블록 끝에서 호출.
	void BuildDynamicCommands(const FFrameContext& Frame, const FScene* Scene);

	// --- Render phase: 정렬 + GPU 제출 ---
	void BeginFrame();
	void EndFrame();

	void RunRootPipeline(ERenderPipelineType RootType, const FFrameContext& Frame);
	void ExecutePipeline(ERenderPipelineType Type, const FFrameContext& Frame);

	// Pass execution helpers — individual pass classes call these.
	void ExecuteDepthPrePass(const FFrameContext& Frame);
	void ExecuteBaseDrawPass(const FFrameContext& Frame);
	void ExecuteDecalPass(const FFrameContext& Frame);
	void ExecuteLightingPass(const FFrameContext& Frame);
	void ExecuteAdditiveDecalPass(const FFrameContext& Frame);
	void ExecuteAlphaBlendPass(const FFrameContext& Frame);
	void ExecuteHeightFogPass(const FFrameContext& Frame);
	void ExecuteFXAAPass(const FFrameContext& Frame);
	void ExecuteSelectionMaskPass(const FFrameContext& Frame);
	void ExecuteOutlinePass(const FFrameContext& Frame);
	void ExecuteDebugLinesPass(const FFrameContext& Frame);
	void ExecuteGizmoRenderPass(const FFrameContext& Frame);
	void ExecuteOverlayFontRenderPass(const FFrameContext& Frame);

	FD3DDevice& GetFD3DDevice() { return Device; }
	FRenderResources& GetResources() { return Resources; }

	const FPassRenderState& GetPassRenderState(ERenderPass Pass) const { return PassRenderStates[(uint32)Pass]; }

private:
	void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FFrameContext& Frame);
	void PreparePipelineExecution(const FFrameContext& Frame);
	void SubmitRenderPass(ERenderPass Pass);
	void SubmitRenderPassByUserBits(ERenderPass Pass, uint16 UserBits);
	void FinalizePipelineExecution();


	// 동적 지오메트리 (DebugLine, Grid, OverlayText) → 라인/폰트 헬퍼
	void PrepareDynamicGeometry(const FFrameContext& Frame, const FScene* Scene);

	// 동적 지오메트리 + PostProcess → FDrawCommand (VB 업로드 + 커맨드 생성)
	void BuildDynamicDrawCommands(const FFrameContext& Frame, ID3D11DeviceContext* Ctx, const FScene* Scene);

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

	// Pipeline/Pass ownership lives outside the renderer.
	FRenderPassRegistry PassRegistry;
	FRenderPipelineRegistry PipelineRegistry;
	FRenderPipelineRunner PipelineRunner;

	// FDrawCommand 기반 렌더링
	FDrawCommandList DrawCommandList;

	TArray<FConstantBuffer> PerObjectCBPool;

	FPassRenderState PassRenderStates[(uint32)ERenderPass::MAX];

	// BeginCollect에서 저장, BuildCommandForProxy에서 사용
	EViewMode CollectViewMode = EViewMode::Lit_Phong;
	bool bHasSelectionMaskCommands = false;

		EViewMode ActiveViewMode = EViewMode::Lit_Phong;
	bool bHasActiveViewModePassConfig = false;
	FViewModeSurfaceSet* OwnedViewModeSurfaceSet = nullptr;
	FViewModeSurfaceSet* ActiveViewSurfaceSet = nullptr;
	FViewModePassRegistry* ViewModePassRegistry = nullptr;

	bool bPipelineExecutionPrepared = false;
	FStateCache PipelineStateCache;
	TArray<FPassEvent> PipelinePrePassEvents;
};
