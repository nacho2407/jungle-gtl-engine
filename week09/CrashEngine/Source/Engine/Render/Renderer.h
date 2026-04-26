// Declares the central renderer that owns shared render systems and frame execution.
#pragma once

#include <memory>
#include <unordered_map>

// ==================== Render Context ====================

#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"

// ==================== Render Pipeline / Pass Registry ====================

#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Registry/RenderPipelineRegistry.h"
#include "Render/Execute/Runner/RenderPipelineRunner.h"

// ==================== Render Resources ====================

#include "Render/Resources/FrameResources.h"
#include "Render/Resources/Shaders/ShaderManager.h"

// ==================== RHI / Device ====================

#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"

// ==================== Submission / Collection ====================

#include "Render/Submission/Batching/OverlayBatchSet.h"
#include "Render/Submission/Collect/DrawCollector.h"
#include "Render/Submission/Command/DrawCommandList.h"

// ==================== Visibility / Culling ====================

#include "Render/Visibility/LightCulling/TileBasedLightCulling.h"

// ==================== Forward Declarations ====================

class FScene;
class FViewModePassRegistry;
class FViewport;
class UWorld;
class FOverlayStatSystem;
class UEditorEngine;
class FOctree;
class FScenePrimitiveBVH;

// FRenderer owns shared render subsystems and drives collection, command building, and pass execution.
class FRenderer
{
public:
    // ==================== Lifetime ====================

    ~FRenderer();

    void Create(HWND hWindow);
    void Release();

    // ==================== View Mode Surface Management ====================

    FViewModeSurfaces* AcquireViewModeSurfaces(FViewport* Viewport, uint32 Width, uint32 Height);
    void               ReleaseViewModeSurfaces(FViewport* Viewport = nullptr);

    const FViewModePassRegistry* GetViewModePassRegistry() const { return ViewModePassRegistry; }
    const FRenderPassRegistry&   GetPassRegistry() const { return PassRegistry; }

    // ==================== Scene Collection ====================

    void BeginCollect(const FSceneView& SceneView, uint32 MaxProxyCount = 0);

    void CollectWorld(UWorld* World, FRenderCollectContext& CollectContext);
    void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene);
    void CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene);
    void CollectDebugRender(const FScene& Scene);

    // ==================== Debug Geometry Collection ====================

    void CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth = 0);
    void CollectScenePrimitiveBVHDebug(const FScenePrimitiveBVH& BVH, FScene& Scene);
    void CollectWorldBoundsDebug(const TArray<class FPrimitiveProxy*>& Proxies, FScene& Scene);

    // ==================== Collected Data Accessors ====================

    const FCollectedSceneData&            GetCollectedSceneData() const { return DrawCollector.GetCollectedSceneData(); }
    const FCollectedPrimitives&           GetCollectedPrimitives() const { return DrawCollector.GetCollectedPrimitives(); }
    const TArray<class FPrimitiveProxy*>& GetLastVisiblePrimitiveProxies() const { return DrawCollector.GetLastVisiblePrimitiveProxies(); }
    const FCollectedLights&               GetCollectedLights() const { return DrawCollector.GetCollectedLights(); }

    // ==================== Draw Command Build ====================

    void BuildDrawCommands(FRenderPipelineContext& PipelineContext);

    // ==================== Frame Execution ====================

    void BeginFrame(const FSceneView& SceneView, const FViewportRenderTargets* Targets = nullptr);
    void EndFrame();
    void RenderFrame(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext);

    // ==================== Pipeline Context Creation ====================

    FRenderPipelineContext CreatePipelineContext(
        const FSceneView&             SceneView,
        const FViewportRenderTargets* Targets = nullptr,
        FScene*                       Scene   = nullptr);

    FRenderPipelineContext CreatePipelineContext(
        const FSceneView&             SceneView,
        const FViewportRenderTargets* Targets,
        FScene*                       Scene,
        const FCollectedSceneData&    SceneData);

    // ==================== Pipeline Execution ====================

    void RunRootPipeline(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext);
    void ExecutePipeline(ERenderPipelineType Type, FRenderPipelineContext& PipelineContext);
    void ExecutePresentPass(FRenderPipelineContext& PipelineContext);

    // ==================== Device / Resource Accessors ====================

    FD3DDevice&      GetFD3DDevice() { return Device; }
    FFrameResources& GetResources() { return FrameResources; }
    FFontBatch&      GetTextBatch() { return FrameResources.TextBatch; }
    FLineBatch&      GetEditorLineBatch() { return OverlayBatches.DebugLines; }
    FLineBatch&      GetGridLineBatch() { return OverlayBatches.GridLines; }

    // ==================== Pass Preset / Constant Buffer Accessors ====================

    const FRenderPassPreset&     GetRenderPassPreset(ERenderPass Pass) const { return PassRegistry.GetRenderPassPreset(Pass); }
    const FRenderPassDrawPreset& GetRenderPassDrawPreset(ERenderPass Pass) const { return PassRegistry.GetRenderPassDrawPreset(Pass); }
    FConstantBuffer*             AcquirePerObjectCBForProxy(const FPrimitiveProxy& Proxy);

private:
    friend struct FRenderPipelineContext;

    // ==================== Frame Preparation Helpers ====================

    void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FSceneView& SceneView);
    void PreparePipelineExecution(const FSceneView& SceneView, const FViewportRenderTargets* Targets);
    void FinalizePipelineExecution();

    // ==================== Pass State Cleanup ====================

    void CleanupPassState(ID3D11DeviceContext* Context, FDrawBindStateCache& Cache);

private:
    // ==================== Device / Shared Frame Resources ====================

    FD3DDevice      Device;
    FFrameResources FrameResources;

    // ==================== Pipeline / Pass Execution System ====================

    FRenderPassRegistry     PassRegistry;
    FRenderPipelineRegistry PipelineRegistry;
    FRenderPipelineRunner   PipelineRunner;

    // ==================== Submission Data ====================

    FDrawCollector   DrawCollector;
    FDrawCommandList DrawCommandList;

    // ==================== View Mode Resources ====================

    std::unordered_map<FViewport*, std::unique_ptr<FViewModeSurfaces>> ViewModeSurfacesMap;
    FViewModePassRegistry*                                             ViewModePassRegistry = nullptr;

    // ==================== Overlay / Lighting Helpers ====================

    FOverlayBatchSet                        OverlayBatches;
    std::unique_ptr<FTileBasedLightCulling> LightCulling;

    // ==================== Active Pipeline State ====================

    bool                bPipelineExecutionPrepared = false;
    FDrawBindStateCache SubmitStateCache;
    const FScene*       ActiveScene = nullptr;
};
