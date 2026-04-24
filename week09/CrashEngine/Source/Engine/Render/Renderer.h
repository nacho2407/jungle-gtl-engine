// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include <memory>
#include <unordered_map>

#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Registry/RenderPipelineRegistry.h"
#include "Render/Execute/Runner/RenderPipelineRunner.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"
#include "Render/Submission/Batching/OverlayBatchSet.h"
#include "Render/Submission/Collect/DrawCollector.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Visibility/LightCulling/TileBasedLightCulling.h"

class FScene;
class FViewModePassRegistry;
class FViewport;
class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FOctree;
class FWorldPrimitivePickingBVH;

// FRenderer는 렌더 영역의 핵심 동작을 담당합니다.
class FRenderer
{
public:
    ~FRenderer();

    void Create(HWND hWindow);
    void Release();

    FViewModeSurfaces* AcquireViewModeSurfaces(FViewport* Viewport, uint32 Width, uint32 Height);
    void               ReleaseViewModeSurfaces(FViewport* Viewport = nullptr);

    const FViewModePassRegistry* GetViewModePassRegistry() const { return ViewModePassRegistry; }
    const FRenderPassRegistry&   GetPassRegistry() const { return PassRegistry; }

    void BeginCollect(const FSceneView& SceneView, uint32 MaxProxyCount = 0);

    void CollectWorld(UWorld* World, FRenderCollectContext& CollectContext);
    void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene);
    void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene);
    void CollectDebugRender(const FScene& Scene);
    void CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth = 0);
    void CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH, FScene& Scene);
    void CollectWorldBoundsDebug(const TArray<class FPrimitiveSceneProxy*>& Proxies, FScene& Scene);

    const FCollectedSceneData&                 GetCollectedSceneData() const { return DrawCollector.GetCollectedSceneData(); }
    const FCollectedPrimitives&                GetCollectedPrimitives() const { return DrawCollector.GetCollectedPrimitives(); }
    const TArray<class FPrimitiveSceneProxy*>& GetLastVisiblePrimitiveProxies() const { return DrawCollector.GetLastVisiblePrimitiveProxies(); }
    const FCollectedLights&                    GetCollectedLights() const { return DrawCollector.GetCollectedLights(); }

    void BuildDrawCommands(FRenderPipelineContext& PipelineContext);

    void BeginFrame(const FSceneView& SceneView, const FViewportRenderTargets* Targets = nullptr);
    void EndFrame();
    void RenderFrame(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext);

    FRenderPipelineContext CreatePipelineContext(
        const FSceneView&             SceneView,
        const FViewportRenderTargets* Targets = nullptr,
        FScene*                       Scene   = nullptr);

    FRenderPipelineContext CreatePipelineContext(
        const FSceneView&             SceneView,
        const FViewportRenderTargets* Targets,
        FScene*                       Scene,
        const FCollectedSceneData&    SceneData);

    void RunRootPipeline(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext);
    void ExecutePipeline(ERenderPipelineType Type, FRenderPipelineContext& PipelineContext);
    void ExecutePresentPass(FRenderPipelineContext& PipelineContext);

    FD3DDevice&      GetFD3DDevice() { return Device; }
    FFrameResources& GetResources() { return FrameResources; }
    FFontBatch&      GetTextBatch() { return FrameResources.TextBatch; }
    FLineBatch&      GetEditorLineBatch() { return OverlayBatches.DebugLines; }
    FLineBatch&      GetGridLineBatch() { return OverlayBatches.GridLines; }

    const FRenderPassPreset&     GetRenderPassPreset(ERenderPass Pass) const { return PassRegistry.GetRenderPassPreset(Pass); }
    const FRenderPassDrawPreset& GetRenderPassDrawPreset(ERenderPass Pass) const { return PassRegistry.GetRenderPassDrawPreset(Pass); }
    FConstantBuffer*             AcquirePerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy);

private:
    friend struct FRenderPipelineContext;

    void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FSceneView& SceneView);
    void PreparePipelineExecution(const FSceneView& SceneView, const FViewportRenderTargets* Targets);
    void FinalizePipelineExecution();
    void CleanupPassState(ID3D11DeviceContext* Context, FDrawBindStateCache& Cache);

private:
    FD3DDevice      Device;
    FFrameResources FrameResources;

    FRenderPassRegistry     PassRegistry;
    FRenderPipelineRegistry PipelineRegistry;
    FRenderPipelineRunner   PipelineRunner;

    FDrawCollector   DrawCollector;
    FDrawCommandList DrawCommandList;

    std::unordered_map<FViewport*, std::unique_ptr<FViewModeSurfaces>> ViewModeSurfacesMap;
    FViewModePassRegistry*                                             ViewModePassRegistry = nullptr;

    FOverlayBatchSet                        OverlayBatches;
    std::unique_ptr<FTileBasedLightCulling> LightCulling;

    bool                bPipelineExecutionPrepared = false;
    FDrawBindStateCache SubmitStateCache;
    const FScene*       ActiveScene = nullptr;
};
