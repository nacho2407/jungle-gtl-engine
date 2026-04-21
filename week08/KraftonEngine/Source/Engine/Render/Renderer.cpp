#include "Render/Renderer.h"
#include <algorithm>
#include <functional>
#include <iostream>

#include "Engine/Runtime/Engine.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Materials/MaterialManager.h"
#include "Profiling/GPUProfiler.h"
#include "Profiling/Stats.h"
#include "Profiling/Timer.h"
#include "Render/Passes/Scene/FogParams.h"
#include "Render/Pipelines/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Buffers/ConstantBufferPool.h"
#include "Render/Pipelines/Runner/PipelineShaderResolver.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Scene/Proxies/Primitive/DecalSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Render/Scene/Scene.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Visibility/TileBasedLightCulling.h"
#include "Resource/ResourceManager.h"

#include <Collision/SpatialPartition.h>

namespace
{
ERenderPassNodeType MapPassToNodeType(ERenderPass Pass)
{
    switch (Pass)
    {
    case ERenderPass::DepthPre:
        return ERenderPassNodeType::DepthPrePass;
    case ERenderPass::Opaque:
        return ERenderPassNodeType::BaseDrawPass;
    case ERenderPass::Decal:
        return ERenderPassNodeType::DecalPass;
    case ERenderPass::Lighting:
        return ERenderPassNodeType::LightingPass;
    case ERenderPass::AdditiveDecal:
        return ERenderPassNodeType::AdditiveDecalPass;
    case ERenderPass::AlphaBlend:
        return ERenderPassNodeType::AlphaBlendPass;
    case ERenderPass::SelectionMask:
        return ERenderPassNodeType::SelectionMaskPass;
    case ERenderPass::EditorLines:
        return ERenderPassNodeType::DebugLinePass;
    case ERenderPass::FXAA:
        return ERenderPassNodeType::FXAAPass;
    case ERenderPass::GizmoOuter:
    case ERenderPass::GizmoInner:
        return ERenderPassNodeType::GizmoPass;
    case ERenderPass::OverlayFont:
        return ERenderPassNodeType::OverlayTextPass;
    case ERenderPass::PostProcess:
        return ERenderPassNodeType::HeightFogPass;
    default:
        return ERenderPassNodeType::BaseDrawPass;
    }
}
} // namespace

FRenderer::~FRenderer() = default;

void FRenderer::Create(HWND hWindow)
{
    Device.Create(hWindow);

    if (Device.GetDevice() == nullptr)
    {
        std::cout << "Failed to create D3D Device." << std::endl;
    }

    FShaderManager::Get().Initialize(Device.GetDevice());
    FConstantBufferPool::Get().Initialize(Device.GetDevice());
    FrameResources.Create(Device.GetDevice());

    PassRegistry.Initialize();
    PipelineRegistry.Initialize();

    ViewModePassRegistry = new FViewModePassRegistry();
    ViewModePassRegistry->Initialize(Device.GetDevice());

    OverlayBatches.GridLines.Create(Device.GetDevice());
    OverlayBatches.DebugLines.Create(Device.GetDevice());

    LightCulling = std::make_unique<FTileBasedLightCulling>();
    LightCulling->Initialize(&Device);

    FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
    if (LightCulling)
    {
        LightCulling->Release();
        LightCulling.reset();
    }

    if (ViewModePassRegistry)
    {
        ViewModePassRegistry->Release();
        delete ViewModePassRegistry;
        ViewModePassRegistry = nullptr;
    }

    ReleaseViewModeSurfaces(nullptr);

    FGPUProfiler::Get().Shutdown();
    PassRegistry.Release();
    PipelineRegistry.Release();

    OverlayBatches.DebugLines.Release();
    OverlayBatches.GridLines.Release();

    FrameResources.Release();
    FConstantBufferPool::Get().Release();
    FShaderManager::Get().Release();
    FMaterialManager::Get().Release();
    Device.Release();
}

FSceneViewModeSurfaces* FRenderer::AcquireViewModeSurfaces(FViewport* Viewport, uint32 Width, uint32 Height)
{
    if (!Viewport || !Device.GetDevice() || Width == 0 || Height == 0)
    {
        return nullptr;
    }

    std::unique_ptr<FSceneViewModeSurfaces>& Entry = ViewModeSurfacesMap[Viewport];
    if (!Entry)
    {
        Entry = std::make_unique<FSceneViewModeSurfaces>();
    }

    Entry->Resize(Device.GetDevice(), Width, Height);
    return Entry.get();
}

void FRenderer::ReleaseViewModeSurfaces(FViewport* Viewport)
{
    if (!Viewport)
    {
        for (auto& Pair : ViewModeSurfacesMap)
        {
            if (Pair.second)
            {
                Pair.second->Release();
            }
        }
        ViewModeSurfacesMap.clear();
        return;
    }

    auto It = ViewModeSurfacesMap.find(Viewport);
    if (It == ViewModeSurfacesMap.end())
    {
        return;
    }

    if (It->second)
    {
        It->second->Release();
    }

    ViewModeSurfacesMap.erase(It);
}

void FRenderer::BeginCollect(const FSceneView& SceneView, uint32 MaxProxyCount)
{
    (void)SceneView;
    DrawCollector.Reset();
    DrawCommandList.Reset();
    ActiveScene = nullptr;
    OverlayBatches.GridLines.Clear();
    OverlayBatches.DebugLines.Clear();
    FrameResources.TextBatch.ClearAll();

    if (MaxProxyCount > 0)
    {
        FrameResources.EnsurePerObjectCBPoolCapacity(Device.GetDevice(), MaxProxyCount);
    }

    if (const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default")))
    {
        FrameResources.EnsureTextCharInfoMap(FontRes);
    }
}

void FRenderer::CollectWorld(UWorld* World, FRenderCollectContext& CollectContext)
{
    if (CollectContext.Scene)
    {
        ActiveScene = CollectContext.Scene;
    }
    DrawCollector.CollectWorld(World, CollectContext);
}

void FRenderer::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene)
{
    (void)Scene;
    DrawCollector.CollectGrid(GridSpacing, GridHalfLineCount);
}

void FRenderer::CollectOverlayText(const FOverlayStatSystem& OverlaySystem, const UEditorEngine& Editor, FScene& Scene)
{
    (void)Scene;
    DrawCollector.CollectOverlayText(OverlaySystem, Editor);
}

void FRenderer::CollectDebugDraw(const FSceneView& SceneView, const FScene& Scene)
{
    DrawCollector.CollectDebugDraw(SceneView, Scene);
}

void FRenderer::CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth)
{
    (void)Scene;
    DrawCollector.CollectOctreeDebug(Node, Depth);
}

void FRenderer::CollectWorldBVHDebug(const FWorldPrimitivePickingBVH& BVH, FScene& Scene)
{
    (void)Scene;
    DrawCollector.CollectWorldBVHDebug(BVH);
}

void FRenderer::CollectWorldBoundsDebug(const TArray<FPrimitiveSceneProxy*>& Proxies, FScene& Scene)
{
    (void)Scene;
    DrawCollector.CollectWorldBoundsDebug(Proxies);
}

void FRenderer::BeginFrame()
{
    FShaderManager::Get().TickHotReload();

    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    ID3D11RenderTargetView* RTV = Device.GetFrameBufferRTV();
    ID3D11DepthStencilView* DSV = Device.GetDepthStencilView();

    Context->ClearRenderTargetView(RTV, Device.GetClearColor());
    Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

    const D3D11_VIEWPORT& Viewport = Device.GetViewport();
    Context->RSSetViewports(1, &Viewport);
    Context->OMSetRenderTargets(1, &RTV, DSV);
}

void FRenderer::EndFrame()
{
    Device.Present();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FSceneView& SceneView)
{
    FFrameConstants FrameConstantData = {};
    FrameConstantData.View = SceneView.View;
    FrameConstantData.Projection = SceneView.Proj;
    FrameConstantData.InvViewProj = (SceneView.View * SceneView.Proj).GetInverse();
    FrameConstantData.bIsWireframe = (SceneView.ViewMode == EViewMode::Wireframe);
    FrameConstantData.WireframeColor = SceneView.WireframeColor;
    FrameConstantData.CameraWorldPos = SceneView.CameraPosition;

    if (GEngine && GEngine->GetTimer())
    {
        FrameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
    }

    FrameResources.FrameBuffer.Update(Context, &FrameConstantData, sizeof(FFrameConstants));
    ID3D11Buffer* FrameCB = FrameResources.FrameBuffer.GetBuffer();
    Context->VSSetConstantBuffers(ECBSlot::Frame, 1, &FrameCB);
    Context->PSSetConstantBuffers(ECBSlot::Frame, 1, &FrameCB);

    const FCollectedLights EmptyLights = {};
    const FCollectedLights& Lights = ActiveScene ? DrawCollector.GetCollectedSceneData().Lights : EmptyLights;

    FrameResources.GlobalLightBuffer.Update(Context, &Lights.GlobalLights, sizeof(FGlobalLightConstants));
    FrameResources.UpdateLocalLights(Device.GetDevice(), Context, Lights.LocalLights);
}

void FRenderer::PreparePipelineExecution(const FSceneView& SceneView, const FViewportRenderTargets* Targets)
{
    FDrawCallStats::Reset();

    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    {
        SCOPE_STAT_CAT("UpdateFrameBuffer", "4_ExecutePass");
        UpdateFrameBuffer(Context, SceneView);
    }

    FrameResources.BindSystemSamplers(Context);
    DrawCommandList.Sort();

    SubmitStateCache.Reset();
    SubmitStateCache.RTV = (Targets && Targets->ViewportRTV) ? Targets->ViewportRTV : Device.GetFrameBufferRTV();
    SubmitStateCache.DSV = (Targets && Targets->ViewportDSV) ? Targets->ViewportDSV : Device.GetDepthStencilView();

    bPipelineExecutionPrepared = true;
}

void FRenderer::FinalizePipelineExecution()
{
    if (!bPipelineExecutionPrepared)
    {
        return;
    }

    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    CleanupPassState(Context, SubmitStateCache);
    ActiveScene = nullptr;
    bPipelineExecutionPrepared = false;
}

void FRenderer::CleanupPassState(ID3D11DeviceContext* Context, FDrawSubmitStateCache& Cache)
{
    ID3D11ShaderResourceView* NullSRVs[8] = {};
    Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
    Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
    Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &NullSRV);
    Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &NullSRV);

    Cache.Cleanup(Context);
    DrawCommandList.Reset();
}

FConstantBuffer* FRenderer::AcquirePerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy)
{
    return FrameResources.GetPerObjectCBForProxy(Device.GetDevice(), Proxy);
}

FRenderPipelineContext FRenderer::CreatePassContext(
    const FSceneView& SceneView,
    const FViewportRenderTargets* Targets,
    FScene* Scene,
    const TArray<FPrimitiveSceneProxy*>* VisibleProxies)
{
    FRenderPipelineContext PipelineContext = {};
    PipelineContext.SceneView = &SceneView;
    PipelineContext.Targets = Targets;
    PipelineContext.Scene = Scene ? Scene : const_cast<FScene*>(ActiveScene);
    PipelineContext.Renderer = this;
    PipelineContext.Device = &Device;
    PipelineContext.Context = Device.GetDeviceContext();
    PipelineContext.Resources = &FrameResources;
    PipelineContext.StateCache = &SubmitStateCache;
    PipelineContext.DrawCommandList = &DrawCommandList;
    PipelineContext.PassStateDescs = PassRegistry.GetPassStateDescs();
    PipelineContext.ViewModePassRegistry = ViewModePassRegistry;
    PipelineContext.ActiveViewSurfaces = nullptr;
    PipelineContext.ActiveViewMode = SceneView.ViewMode;
    PipelineContext.CollectedPrimitives = nullptr;
    PipelineContext.VisibleProxies = VisibleProxies;
    PipelineContext.OverlayData = &DrawCollector.GetCollectedOverlayData();
    PipelineContext.DebugLines = &DrawCollector.GetCollectedOverlayData().GetDebugLines();
    PipelineContext.OverlayTexts = &DrawCollector.GetCollectedPrimitives().OverlayTexts;
    PipelineContext.Occlusion = SceneView.OcclusionCulling;
    PipelineContext.LightCulling = LightCulling.get();
    PipelineContext.LODContext = &SceneView.LODContext;
    return PipelineContext;
}

FRenderPipelineContext FRenderer::CreatePassContext(
    const FSceneView& SceneView,
    const FViewportRenderTargets* Targets,
    FScene* Scene,
    const FCollectedPrimitives& CollectedPrimitives)
{
    FRenderPipelineContext PipelineContext = CreatePassContext(SceneView, Targets, Scene, &CollectedPrimitives.VisibleProxies);
    PipelineContext.CollectedPrimitives = &CollectedPrimitives;
    return PipelineContext;
}

void FRenderer::BuildDrawCommands(FRenderPipelineContext& PipelineContext)
{
    if (!PipelineContext.SceneView || !PipelineContext.Scene || !PipelineContext.DrawCommandList)
    {
        return;
    }

    ActiveScene = PipelineContext.Scene;

    const FCollectedPrimitives& CollectedPrimitives = DrawCollector.GetCollectedSceneData().Primitives;
    PipelineContext.CollectedPrimitives = &CollectedPrimitives;
    PipelineContext.VisibleProxies = &CollectedPrimitives.VisibleProxies;

    const FViewModePassRegistry* ViewModeRegistry = PipelineContext.ViewModePassRegistry ? PipelineContext.ViewModePassRegistry : ViewModePassRegistry;
    const bool bHasViewModeConfig = ViewModeRegistry && ViewModeRegistry->HasConfig(PipelineContext.ActiveViewMode);
    const bool bUsesDepthPre = bHasViewModeConfig && ViewModeRegistry->UsesDepthPrePass(PipelineContext.ActiveViewMode);
    const bool bUsesBaseDraw = bHasViewModeConfig && ViewModeRegistry->UsesBaseDraw(PipelineContext.ActiveViewMode);
    const bool bUsesDecal = bHasViewModeConfig && ViewModeRegistry->UsesDecal(PipelineContext.ActiveViewMode);
    const bool bUsesAdditiveDecal = bHasViewModeConfig && ViewModeRegistry->UsesAdditiveDecal(PipelineContext.ActiveViewMode);
    const bool bUsesAlphaBlend = bHasViewModeConfig && ViewModeRegistry->UsesAlphaBlend(PipelineContext.ActiveViewMode);

    TSet<FPrimitiveSceneProxy*> VisibleProxySet;
    VisibleProxySet.reserve(CollectedPrimitives.VisibleProxies.size());
    for (FPrimitiveSceneProxy* Proxy : CollectedPrimitives.VisibleProxies)
    {
        if (Proxy)
        {
            VisibleProxySet.insert(Proxy);
        }
    }

    for (FPrimitiveSceneProxy* Proxy : CollectedPrimitives.VisibleProxies)
    {
        if (!Proxy)
        {
            continue;
        }

        if (Proxy->bFontBatched)
        {
            const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
            if (!TextProxy->CachedText.empty())
            {
                DrawCommandBuilder::BuildWorldTextDrawCommand(*TextProxy, PipelineContext, *PipelineContext.DrawCommandList);
            }
        }
        else if (Cast<UDecalComponent>(Proxy->Owner))
        {
            FDecalSceneProxy* DecalProxy = static_cast<FDecalSceneProxy*>(Proxy);

            if (bHasViewModeConfig)
            {
                if (bUsesDecal)
                {
                    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::DecalPass))
                    {
                        Pass->BuildDrawCommands(PipelineContext, *DecalProxy);
                    }
                }
            }
            else
            {
                UDecalComponent* DecalComponent = static_cast<UDecalComponent*>(Proxy->Owner);
                for (UStaticMeshComponent* Receiver : DecalComponent->GetReceivers())
                {
                    if (!Receiver)
                    {
                        continue;
                    }
                    FPrimitiveSceneProxy* ReceiverProxy = Receiver->GetSceneProxy();
                    if (!ReceiverProxy || VisibleProxySet.find(ReceiverProxy) == VisibleProxySet.end())
                    {
                        continue;
                    }
                    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::DecalPass))
                    {
                        Pass->BuildDrawCommands(PipelineContext, *ReceiverProxy);
                    }
                }
            }
        }
        else
        {
            if (bUsesDepthPre && Proxy->Pass == ERenderPass::Opaque && PassRegistry.FindPass(ERenderPassNodeType::DepthPrePass))
            {
                DrawCommandBuilder::BuildMeshDrawCommand(*Proxy, ERenderPass::DepthPre, PipelineContext, *PipelineContext.DrawCommandList);
            }

            if (bHasViewModeConfig)
            {
                if (Proxy->Pass == ERenderPass::Opaque && !bUsesBaseDraw)
                {
                    continue;
                }
                if (Proxy->Pass == ERenderPass::Decal && !bUsesDecal)
                {
                    continue;
                }
                if (Proxy->Pass == ERenderPass::AdditiveDecal && !bUsesAdditiveDecal)
                {
                    continue;
                }
                if (Proxy->Pass == ERenderPass::AlphaBlend && !bUsesAlphaBlend)
                {
                    continue;
                }
            }

            if (FRenderPass* Pass = PassRegistry.FindPass(MapPassToNodeType(Proxy->Pass)))
            {
                Pass->BuildDrawCommands(PipelineContext, *Proxy);
            }
        }

        if (Proxy->bSelected && Proxy->bSupportsOutline)
        {
            if (FRenderPass* SelectionMaskPass = PassRegistry.FindPass(ERenderPassNodeType::SelectionMaskPass))
            {
                SelectionMaskPass->BuildDrawCommands(PipelineContext, *Proxy);
            }
        }
    }

    if (bHasViewModeConfig)
    {
        if (ViewModeRegistry->UsesLightingPass(PipelineContext.ActiveViewMode))
        {
            if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::LightingPass))
            {
                Pass->BuildDrawCommands(PipelineContext);
            }
        }

        if (ViewModeRegistry->UsesViewModeResolve(PipelineContext.ActiveViewMode))
        {
            if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::ViewModeResolvePass))
            {
                Pass->BuildDrawCommands(PipelineContext);
            }
        }

        if (ViewModeRegistry->UsesHeightFog(PipelineContext.ActiveViewMode))
        {
            if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::HeightFogPass))
            {
                Pass->BuildDrawCommands(PipelineContext);
            }
        }

        if (ViewModeRegistry->UsesFXAA(PipelineContext.ActiveViewMode))
        {
            if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::FXAAPass))
            {
                Pass->BuildDrawCommands(PipelineContext);
            }
        }
    }

    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::OutlinePass))
    {
        Pass->BuildDrawCommands(PipelineContext);
    }
    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::DebugLinePass))
    {
        Pass->BuildDrawCommands(PipelineContext);
    }
    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::OverlayTextPass))
    {
        Pass->BuildDrawCommands(PipelineContext);
    }
}

void FRenderer::RunRootPipeline(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext)
{
    const bool bRootToBackBuffer = !(PipelineContext.Targets && PipelineContext.Targets->ViewportRTV);
    if (bRootToBackBuffer)
    {
        BeginFrame();
    }

    PreparePipelineExecution(*PipelineContext.SceneView, PipelineContext.Targets);
    PipelineContext.Renderer = this;
    PipelineContext.StateCache = &SubmitStateCache;
    PipelineContext.Context = Device.GetDeviceContext();
    PipelineContext.Device = &Device;
    PipelineContext.Resources = &FrameResources;
    PipelineContext.DrawCommandList = &DrawCommandList;
    PipelineContext.PassStateDescs = PassRegistry.GetPassStateDescs();
    PipelineContext.ViewModePassRegistry = ViewModePassRegistry;
    PipelineContext.LightCulling = LightCulling.get();

    PipelineRunner.ExecutePipeline(RootType, PipelineContext, *PipelineContext.SceneView, PipelineRegistry, PassRegistry);
    FinalizePipelineExecution();

    if (bRootToBackBuffer)
    {
        EndFrame();
    }
}

void FRenderer::ExecutePipeline(ERenderPipelineType Type, FRenderPipelineContext& PipelineContext)
{
    PipelineContext.Renderer = this;
    PipelineContext.StateCache = &SubmitStateCache;
    PipelineContext.Context = Device.GetDeviceContext();
    PipelineContext.Device = &Device;
    PipelineContext.Resources = &FrameResources;
    PipelineContext.DrawCommandList = &DrawCommandList;
    PipelineContext.PassStateDescs = PassRegistry.GetPassStateDescs();
    PipelineContext.ViewModePassRegistry = ViewModePassRegistry;
    PipelineContext.LightCulling = LightCulling.get();

    PipelineRunner.ExecutePipeline(Type, PipelineContext, *PipelineContext.SceneView, PipelineRegistry, PassRegistry);
}
