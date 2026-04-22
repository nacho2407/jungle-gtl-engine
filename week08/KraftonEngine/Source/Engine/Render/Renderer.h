#pragma once

#include <memory>
#include <unordered_map>

#include "Render/Overlay/OverlayBatchSet.h"
#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Pipelines/Context/FrameRenderResources.h"
#include "Render/Pipelines/Context/RenderCollectContext.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Pipelines/Context/ViewMode/SceneViewModeSurfaces.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Pipelines/Registry/RenderPassRegistry.h"
#include "Render/Pipelines/Registry/RenderPipelineRegistry.h"
#include "Render/Pipelines/Runner/RenderPipelineRunner.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"
#include "Render/Submission/Collect/DrawCollector.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Visibility/TileBasedLightCulling.h"

class FScene;
class FViewModePassRegistry;
class FViewport;
class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FOctree;
class FWorldPrimitivePickingBVH;

/*
    렌더링 전체를 총괄하는 최상위 오케스트레이터입니다.
    프레임 공용 자원 준비, 드로우 대상 수집, 커맨드 생성, 파이프라인 실행만 담당합니다.
*/
class FRenderer
{
public:
    ~FRenderer();

    void Create(HWND hWindow);
    void Release();

    FSceneViewModeSurfaces* AcquireViewModeSurfaces(FViewport* Viewport, uint32 Width, uint32 Height);
    void                    ReleaseViewModeSurfaces(FViewport* Viewport = nullptr);

    const FViewModePassRegistry* GetViewModePassRegistry() const { return ViewModePassRegistry; }
    const FRenderPassRegistry&   GetPassRegistry() const { return PassRegistry; }

    void BeginCollect(const FSceneView& SceneView, uint32 MaxProxyCount = 0);

    // 수집 단계 facade
    void CollectWorld(UWorld* World, FRenderCollectContext& CollectContext);
    void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene);
    void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene);
    void CollectDebugDraw(const FSceneView& SceneView, const FScene& Scene);
    void CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth = 0);
    void CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH, FScene& Scene);
    void CollectWorldBoundsDebug(const TArray<class FPrimitiveSceneProxy*>& Proxies, FScene& Scene);

    const FCollectedSceneData&                 GetCollectedSceneData() const { return DrawCollector.GetCollectedSceneData(); }
    const FCollectedPrimitives&                GetCollectedPrimitives() const { return DrawCollector.GetCollectedPrimitives(); }
    const TArray<class FPrimitiveSceneProxy*>& GetLastVisiblePrimitiveProxies() const { return DrawCollector.GetLastVisiblePrimitiveProxies(); }
    const FCollectedLights&                    GetCollectedLights() const { return DrawCollector.GetCollectedLights(); }

    // 수집된 결과를 드로우 커맨드로 변환
    void BuildDrawCommands(FRenderPipelineContext& PipelineContext);

    void BeginFrame(const FSceneView& SceneView, const FViewportRenderTargets* Targets = nullptr);
    void EndFrame();
    void RenderFrame(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext);

    FRenderPipelineContext CreatePassContext(
        const FSceneView&                          SceneView,
        const FViewportRenderTargets*              Targets        = nullptr,
        FScene*                                    Scene          = nullptr,
        const TArray<class FPrimitiveSceneProxy*>* VisibleProxies = nullptr);

    FRenderPipelineContext CreatePassContext(
        const FSceneView&             SceneView,
        const FViewportRenderTargets* Targets,
        FScene*                       Scene,
        const FCollectedPrimitives&   CollectedPrimitives);

    void RunRootPipeline(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext);
    void ExecutePipeline(ERenderPipelineType Type, FRenderPipelineContext& PipelineContext);
    void ExecutePresentPass(FRenderPipelineContext& PipelineContext);

    FD3DDevice&            GetFD3DDevice() { return Device; }
    FFrameRenderResources& GetResources() { return FrameResources; }
    FFontBatch&            GetTextBatch() { return FrameResources.TextBatch; }
    FLineBatch&            GetEditorLineBatch() { return OverlayBatches.DebugLines; }
    FLineBatch&            GetGridLineBatch() { return OverlayBatches.GridLines; }

    const FPassRenderStateDesc& GetPassStateDesc(ERenderPass Pass) const { return PassRegistry.GetPassStateDesc(Pass); }
    FConstantBuffer*            AcquirePerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy);

private:
    friend struct FRenderPipelineContext;

    void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FSceneView& SceneView);
    void PreparePipelineExecution(const FSceneView& SceneView, const FViewportRenderTargets* Targets);
    void FinalizePipelineExecution();
    void CleanupPassState(ID3D11DeviceContext* Context, FDrawSubmitStateCache& Cache);

private:
    FD3DDevice            Device;
    FFrameRenderResources FrameResources;

    FRenderPassRegistry     PassRegistry;
    FRenderPipelineRegistry PipelineRegistry;
    FRenderPipelineRunner   PipelineRunner;

    FDrawCollector   DrawCollector;
    FDrawCommandList DrawCommandList;

    std::unordered_map<FViewport*, std::unique_ptr<FSceneViewModeSurfaces>> ViewModeSurfacesMap;
    FViewModePassRegistry*                                                  ViewModePassRegistry = nullptr;

    FOverlayBatchSet OverlayBatches;
    std::unique_ptr<FTileBasedLightCulling> LightCulling;

    bool                  bPipelineExecutionPrepared = false;
    FDrawSubmitStateCache SubmitStateCache;
    const FScene*         ActiveScene = nullptr;
};
