#pragma once

#include "Render/Pipelines/RenderPassTypes.h"
/*
    실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"

#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Passes/Base/PassRenderState.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"
#include "Render/Pipelines/Context/FrameSharedResources.h"
#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"
#include "Render/Resources/ShaderManager.h"
#include "Render/Submission/Batching/LineBatch.h"
#include "Render/Submission/Batching/FontBatch.h"
#include "Render/Pipelines/Registry/RenderPassRegistry.h"
#include "Render/Pipelines/Registry/RenderPipelineRegistry.h"
#include "Render/Pipelines/RenderPipelineRunner.h"

class FTextRenderSceneProxy;
class FScene;
class FViewModePassRegistry;
class FViewModeSurfaceSet;
struct FCollectedPrimitives;


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
    const FRenderPassRegistry& GetPassRegistry() const { return PassRegistry; }

    // --- Collect phase: Pipeline이 호출하여 커맨드 수집 시작/종료 ---
    // MaxProxyCount: Scene의 프록시 수. PerObjectCBPool을 미리 할당하여
    // Collect 도중 resize로 인한 포인터 무효화를 방지.
    void BeginCollect(const FFrameContext& Frame, uint32 MaxProxyCount = 0);

    void SetCollectedScene(const FScene* InScene) { ActiveSceneForFrame = InScene; }
    void SetCollectedLights(const FCollectedLights* InCollectedLights) { ActiveCollectedLights = InCollectedLights; }

    // --- Render phase: 정렬 + GPU 제출 ---
    void BeginFrame();
    void EndFrame();

    FRenderPipelineContext CreatePassContext(const FFrameContext& Frame, const FViewportRenderTargets* Targets = nullptr, FScene* Scene = nullptr, const TArray<FPrimitiveSceneProxy*>* VisibleProxies = nullptr);
    FRenderPipelineContext CreatePassContext(const FFrameContext& Frame, const FViewportRenderTargets* Targets, FScene* Scene, const FCollectedPrimitives& CollectedPrimitives);
    void RunRootPipeline(ERenderPipelineType RootType, FRenderPipelineContext& PassContext);
    void ExecutePipeline(ERenderPipelineType Type, FRenderPipelineContext& PassContext);

    FD3DDevice& GetFD3DDevice() { return Device; }
    FFrameSharedResources& GetResources() { return Resources; }
    FLineBatch& GetEditorLineBatch() { return EditorLines; }
    FLineBatch& GetGridLineBatch() { return GridLines; }
    FFontBatch& GetFontBatch() { return FontGeometry; }

    const FPassRenderState& GetPassRenderState(ERenderPass Pass) const { return PassRenderStates[(uint32)Pass]; }
    bool HasSelectionMaskCommands() const { return bHasSelectionMaskCommands; }
    FConstantBuffer* AcquirePerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy) { return GetPerObjectCBForProxy(Proxy); }

private:
    friend struct FRenderPipelineContext;
    void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FFrameContext& Frame);
    void PreparePipelineExecution(const FFrameContext& Frame, const FViewportRenderTargets* Targets);
    void FinalizePipelineExecution();


    // 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
    void CleanupPassState(ID3D11DeviceContext* Context, FStateCache& Cache);

    // PerObjectCB 풀 관리
    void EnsurePerObjectCBPoolCapacity(uint32 RequiredCount);
    FConstantBuffer* GetPerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy);

private:
    FD3DDevice Device;
    FFrameSharedResources Resources;
    FLineBatch EditorLines;
    FLineBatch GridLines;
    FFontBatch FontGeometry;

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
    const FScene* ActiveSceneForFrame = nullptr;
    const FCollectedLights* ActiveCollectedLights = nullptr;
};
