// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Renderer.h"

#include <algorithm>
#include <functional>
#include <iostream>

// ==================== Engine / Component ====================

#include "Engine/Runtime/Engine.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"

// ==================== Materials / Resources ====================

#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"

// ==================== Profiling ====================

#include "Profiling/GPUProfiler.h"
#include "Profiling/Stats.h"
#include "Profiling/Timer.h"

// ==================== Render Execute ====================

#include "Render/Execute/Passes/Scene/PresentPass.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"

// ==================== Render Resources ====================

#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"

// ==================== Scene / Proxy ====================

#include "Render/Scene/Proxies/Primitive/DecalSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Render/Scene/Scene.h"

// ==================== Submission / Visibility ====================

#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Visibility/LightCulling/TileBasedLightCulling.h"

#include <Collision/SpatialPartition.h>

// ==================== File Local Helpers ====================

namespace
{
ERenderPassNodeType MapPassToNodeType(ERenderPass Pass, const FRenderPipelineContext& Context)
{
    switch (Pass)
    {
    case ERenderPass::DepthPre:
        return ERenderPassNodeType::DepthPrePass;
    case ERenderPass::Opaque:
        if (Context.SceneView && Context.SceneView->RenderPath == ERenderShadingPath::Forward)
        {
            return ERenderPassNodeType::ForwardOpaquePass;
        }
        return ERenderPassNodeType::DeferredOpaquePass;
    case ERenderPass::Decal:
        if (Context.SceneView && Context.SceneView->RenderPath == ERenderShadingPath::Forward)
        {
            return ERenderPassNodeType::ForwardDecalPass;
        }
        return ERenderPassNodeType::DeferredDecalPass;
    case ERenderPass::DeferredLighting:
        return ERenderPassNodeType::DeferredLightingPass;
    case ERenderPass::AdditiveDecal:
        return ERenderPassNodeType::AdditiveDecalPass;
    case ERenderPass::AlphaBlend:
        return ERenderPassNodeType::AlphaBlendPass;
    case ERenderPass::SelectionMask:
        return ERenderPassNodeType::SelectionMaskPass;
    case ERenderPass::EditorLines:
        return ERenderPassNodeType::DebugLinePass;
    case ERenderPass::OverlayBillboard:
        return ERenderPassNodeType::OverlayBillboardPass;
    case ERenderPass::FXAA:
        return ERenderPassNodeType::FXAAPass;
    case ERenderPass::GizmoOuter:
    case ERenderPass::GizmoInner:
        return ERenderPassNodeType::GizmoPass;
    case ERenderPass::OverlayTextWorld:
    case ERenderPass::OverlayFont:
        return ERenderPassNodeType::OverlayTextPass;
    case ERenderPass::PostProcess:
        return ERenderPassNodeType::HeightFogPass;
    default:
        return ERenderPassNodeType::DeferredOpaquePass;
    }
}
} // namespace

// ==================== Lifetime ====================

FRenderer::~FRenderer() = default;

void FRenderer::Create(HWND hWindow)
{
    Device.Create(hWindow);

    if (Device.GetDevice() == nullptr)
    {
        std::cout << "Failed to create D3D Device." << std::endl;
    }

    // 렌더 전역 리소스 초기화
    FShaderManager::Get().Initialize(Device.GetDevice());
    FConstantBufferCache::Get().Initialize(Device.GetDevice());
    FrameResources.Create(Device.GetDevice());

    // 패스와 파이프라인 정의 초기화
    PassRegistry.Initialize();
    PipelineRegistry.Initialize();

    // 뷰모드별 패스 설정 초기화
    ViewModePassRegistry = new FViewModePassRegistry();
    ViewModePassRegistry->Initialize(Device.GetDevice());

    // 에디터 오버레이 배치 리소스 초기화
    OverlayBatches.GridLines.Create(Device.GetDevice());
    OverlayBatches.DebugLines.Create(Device.GetDevice());

    // 타일 기반 로컬 라이트 컬링 초기화
    LightCulling = std::make_unique<FTileBasedLightCulling>();
    LightCulling->Initialize(&Device);

    // GPU 프로파일러 초기화
    FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
    // 라이트 컬링 리소스 해제
    if (LightCulling)
    {
        LightCulling->Release();
        LightCulling.reset();
    }

    // 뷰모드 패스 레지스트리 해제
    if (ViewModePassRegistry)
    {
        ViewModePassRegistry->Release();
        delete ViewModePassRegistry;
        ViewModePassRegistry = nullptr;
    }

    // 뷰포트별 뷰모드 렌더 타겟 해제
    ReleaseViewModeSurfaces(nullptr);

    // 전역 렌더 시스템 해제
    FGPUProfiler::Get().Shutdown();
    PassRegistry.Release();
    PipelineRegistry.Release();

    // 오버레이 배치 해제
    OverlayBatches.DebugLines.Release();
    OverlayBatches.GridLines.Release();

    // 공유 리소스와 매니저 해제
    FrameResources.Release();
    FConstantBufferCache::Get().Release();
    FShaderManager::Get().Release();
    FMaterialManager::Get().Release();

    // D3D 디바이스 해제
    Device.Release();
}

// ==================== View Mode Surface Management ====================

FViewModeSurfaces* FRenderer::AcquireViewModeSurfaces(FViewport* Viewport, uint32 Width, uint32 Height)
{
    if (!Viewport || !Device.GetDevice() || Width == 0 || Height == 0)
    {
        return nullptr;
    }

    std::unique_ptr<FViewModeSurfaces>& Entry = ViewModeSurfacesMap[Viewport];
    if (!Entry)
    {
        Entry = std::make_unique<FViewModeSurfaces>();
    }

    Entry->Resize(Device.GetDevice(), Width, Height);
    return Entry.get();
}

void FRenderer::ReleaseViewModeSurfaces(FViewport* Viewport)
{
    // Viewport이 없으면 전체 뷰모드 Surface를 해제합니다.
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

    // 특정 Viewport에 연결된 Surface만 해제합니다.
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

// ==================== Scene Collection ====================

void FRenderer::BeginCollect(const FSceneView& SceneView, uint32 MaxProxyCount)
{
    (void)SceneView;

    // 이전 프레임 수집 결과 초기화
    DrawCollector.Reset();
    DrawCommandList.Reset();
    ActiveScene = nullptr;

    // 오버레이 배치 초기화
    OverlayBatches.GridLines.Clear();
    OverlayBatches.DebugLines.Clear();
    FrameResources.TextBatch.ClearAll();

    // 프록시 수가 예측 가능하면 PerObject 상수 버퍼 풀을 미리 확보합니다.
    if (MaxProxyCount > 0)
    {
        FrameResources.EnsurePerObjectCBPoolCapacity(Device.GetDevice(), MaxProxyCount);
    }

    // 텍스트 렌더링에 필요한 기본 폰트 문자 정보를 준비합니다.
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

void FRenderer::CollectDebugRender(const FScene& Scene)
{
    DrawCollector.CollectDebugRender(Scene);
}

// ==================== Debug Geometry Collection ====================

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

void FRenderer::CollectWorldBoundsDebug(const TArray<FPrimitiveProxy*>& Proxies, FScene& Scene)
{
    (void)Scene;
    DrawCollector.CollectWorldBoundsDebug(Proxies);
}

// ==================== Pipeline Context Creation ====================

FRenderPipelineContext FRenderer::CreatePipelineContext(
    const FSceneView&             SceneView,
    const FViewportRenderTargets* Targets,
    FScene*                       Scene)
{
    FRenderPipelineContext PipelineContext  = {};
    PipelineContext.SceneView               = &SceneView;
    PipelineContext.Targets                 = Targets;
    PipelineContext.Scene                   = Scene ? Scene : const_cast<FScene*>(ActiveScene);
    PipelineContext.Renderer                = this;
    PipelineContext.Device                  = &Device;
    PipelineContext.Context                 = Device.GetDeviceContext();
    PipelineContext.Resources               = &FrameResources;
    PipelineContext.StateCache              = &SubmitStateCache;
    PipelineContext.DrawCommandList         = &DrawCommandList;
    PipelineContext.RenderPassPresets       = PassRegistry.GetRenderPassPresets();
    PipelineContext.ViewMode.Registry       = ViewModePassRegistry;
    PipelineContext.ViewMode.Surfaces       = nullptr;
    PipelineContext.ViewMode.ActiveViewMode = SceneView.ViewMode;
    PipelineContext.Submission.SceneData    = &DrawCollector.GetCollectedSceneData();
    PipelineContext.Submission.OverlayData  = &DrawCollector.GetCollectedOverlayData();
    PipelineContext.Occlusion               = SceneView.OcclusionCulling;
    PipelineContext.LightCulling            = LightCulling.get();
    PipelineContext.LODContext              = &SceneView.LODContext;

    if (Targets && Targets->SourceViewport && ViewModePassRegistry &&
        ViewModePassRegistry->HasConfig(SceneView.ViewMode))
    {
        FViewport* Viewport = const_cast<FViewport*>(Targets->SourceViewport);
        PipelineContext.ViewMode.Surfaces =
            AcquireViewModeSurfaces(Viewport, static_cast<uint32>(SceneView.ViewportWidth), static_cast<uint32>(SceneView.ViewportHeight));
    }

    return PipelineContext;
}

FRenderPipelineContext FRenderer::CreatePipelineContext(
    const FSceneView&             SceneView,
    const FViewportRenderTargets* Targets,
    FScene*                       Scene,
    const FCollectedSceneData&    SceneData)
{
    FRenderPipelineContext PipelineContext = CreatePipelineContext(SceneView, Targets, Scene);
    PipelineContext.Submission.SceneData   = &SceneData;

    return PipelineContext;
}

// ==================== Draw Command Build ====================

void FRenderer::BuildDrawCommands(FRenderPipelineContext& PipelineContext)
{
    if (!PipelineContext.SceneView || !PipelineContext.Scene || !PipelineContext.DrawCommandList)
    {
        return;
    }

    ActiveScene = PipelineContext.Scene;

    // 외부에서 SceneData를 지정하지 않은 경우, 현재 DrawCollector의 결과를 사용합니다.
    if (!PipelineContext.Submission.SceneData)
    {
        PipelineContext.Submission.SceneData = &DrawCollector.GetCollectedSceneData();
    }
    if (!PipelineContext.Submission.OverlayData)
    {
        PipelineContext.Submission.OverlayData = &DrawCollector.GetCollectedOverlayData();
    }

    const FCollectedPrimitives& CollectedPrimitives = PipelineContext.Submission.SceneData->Primitives;

    // 현재 ViewMode가 사용할 패스 구성을 조회합니다.
    const FViewModePassRegistry* ViewModeRegistry   = PipelineContext.ViewMode.Registry ? PipelineContext.ViewMode.Registry : ViewModePassRegistry;
    const bool                   bHasViewModeConfig = ViewModeRegistry && ViewModeRegistry->HasConfig(PipelineContext.ViewMode.ActiveViewMode);
    const bool                   bUsesDepthPre      = bHasViewModeConfig && ViewModeRegistry->UsesDepthPrePass(PipelineContext.ViewMode.ActiveViewMode);
    const bool                   bUsesOpaque        = bHasViewModeConfig && ViewModeRegistry->UsesOpaque(PipelineContext.ViewMode.ActiveViewMode);
    const bool                   bUsesDecal         = bHasViewModeConfig && ViewModeRegistry->UsesDecal(PipelineContext.ViewMode.ActiveViewMode);
    const bool                   bUsesAdditiveDecal = bHasViewModeConfig && ViewModeRegistry->UsesAdditiveDecal(PipelineContext.ViewMode.ActiveViewMode);
    const bool                   bUsesAlphaBlend    = bHasViewModeConfig && ViewModeRegistry->UsesAlphaBlend(PipelineContext.ViewMode.ActiveViewMode);

    // Primitive 단위 draw command 생성
    for (FPrimitiveProxy* Proxy : CollectedPrimitives.VisibleProxies)
    {
        if (!Proxy)
        {
            continue;
        }

        // 월드 공간 텍스트 프록시 처리
        if (Proxy->bFontBatched)
        {
            const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
            if (!TextProxy->CachedText.empty())
            {
                DrawCommandBuild::BuildWorldTextDrawCommand(*TextProxy, PipelineContext, *PipelineContext.DrawCommandList);
            }
        }
        // 데칼 프록시 처리
        else if (Cast<UDecalComponent>(Proxy->Owner))
        {
            FDecalSceneProxy* DecalProxy = static_cast<FDecalSceneProxy*>(Proxy);

            if (bHasViewModeConfig && bUsesDecal)
            {
                const ERenderPassNodeType DecalPassType =
                    (PipelineContext.SceneView && PipelineContext.SceneView->RenderPath == ERenderShadingPath::Forward)
                        ? ERenderPassNodeType::ForwardDecalPass
                        : ERenderPassNodeType::DeferredDecalPass;
                if (FRenderPass* Pass = PassRegistry.FindPass(DecalPassType))
                {
                    Pass->BuildDrawCommands(PipelineContext, *DecalProxy);
                }
            }
        }
        // 일반 메시 프록시 처리
        else
        {
            // Depth PrePass가 필요한 ViewMode라면 Opaque 프록시에 대해 먼저 DepthPre 명령을 생성합니다.
            if (bUsesDepthPre && Proxy->Pass == ERenderPass::Opaque && PassRegistry.FindPass(ERenderPassNodeType::DepthPrePass))
            {
                DrawCommandBuild::BuildMeshDrawCommand(*Proxy, ERenderPass::DepthPre, PipelineContext, *PipelineContext.DrawCommandList);
            }

            // ViewMode 설정에서 비활성화된 패스의 프록시는 draw command 생성을 건너뜁니다.
            if (bHasViewModeConfig)
            {
                if (Proxy->Pass == ERenderPass::Opaque && !bUsesOpaque)
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

            if (FRenderPass* Pass = PassRegistry.FindPass(MapPassToNodeType(Proxy->Pass, PipelineContext)))
            {
                Pass->BuildDrawCommands(PipelineContext, *Proxy);
            }
        }

        // 선택된 프록시는 별도의 SelectionMask 명령을 추가합니다.
        if (Proxy->bSelected && Proxy->bSupportsOutline)
        {
            if (FRenderPass* SelectionMaskPass = PassRegistry.FindPass(ERenderPassNodeType::SelectionMaskPass))
            {
                SelectionMaskPass->BuildDrawCommands(PipelineContext, *Proxy);
            }
        }
    }

    // ViewMode 후처리 및 화면 공간 패스 명령 생성
    if (bHasViewModeConfig)
    {
        if (ViewModeRegistry->UsesLightingPass(PipelineContext.ViewMode.ActiveViewMode))
        {
            if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::DeferredLightingPass))
            {
                Pass->BuildDrawCommands(PipelineContext);
            }
        }

        if (ViewModeRegistry->UsesNonLitViewMode(PipelineContext.ViewMode.ActiveViewMode))
        {
            if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::NonLitViewModePass))
            {
                Pass->BuildDrawCommands(PipelineContext);
            }
        }

        if (ViewModeRegistry->UsesHeightFog(PipelineContext.ViewMode.ActiveViewMode))
        {
            if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::HeightFogPass))
            {
                Pass->BuildDrawCommands(PipelineContext);
            }
        }

        if (ViewModeRegistry->UsesFXAA(PipelineContext.ViewMode.ActiveViewMode))
        {
            if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::FXAAPass))
            {
                Pass->BuildDrawCommands(PipelineContext);
            }
        }
    }

    // 에디터 오버레이 및 보조 패스 명령 생성
    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::OutlinePass))
    {
        Pass->BuildDrawCommands(PipelineContext);
    }
    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::DebugLinePass))
    {
        Pass->BuildDrawCommands(PipelineContext);
    }
    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::OverlayBillboardPass))
    {
        Pass->BuildDrawCommands(PipelineContext);
    }
    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::OverlayTextPass))
    {
        Pass->BuildDrawCommands(PipelineContext);
    }
    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::LightHitMapPass))
    {
        Pass->BuildDrawCommands(PipelineContext);
    }
}

// ==================== Frame Execution ====================

void FRenderer::BeginFrame(const FSceneView& SceneView, const FViewportRenderTargets* Targets)
{
    FShaderManager::Get().TickHotReload();

    ID3D11DeviceContext*    Context = Device.GetDeviceContext();
    ID3D11RenderTargetView* RTV     = (Targets && Targets->ViewportRTV) ? Targets->ViewportRTV : Device.GetFrameBufferRTV();
    ID3D11DepthStencilView* DSV     = (Targets && Targets->ViewportDSV) ? Targets->ViewportDSV : Device.GetDepthStencilView();

    // 현재 프레임의 컬러/깊이 타겟 초기화
    if (RTV)
    {
        Context->ClearRenderTargetView(RTV, Device.GetClearColor());
    }
    if (DSV)
    {
        Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
    }

    // 뷰포트 렌더 타겟이 있으면 SceneView 크기에 맞춰 viewport를 재설정합니다.
    D3D11_VIEWPORT Viewport = Device.GetViewport();
    if (Targets && Targets->HasViewportTargets() && SceneView.ViewportWidth > 0.0f && SceneView.ViewportHeight > 0.0f)
    {
        Viewport.TopLeftX = 0.0f;
        Viewport.TopLeftY = 0.0f;
        Viewport.Width    = SceneView.ViewportWidth;
        Viewport.Height   = SceneView.ViewportHeight;
        Viewport.MinDepth = 0.0f;
        Viewport.MaxDepth = 1.0f;
    }

    Context->RSSetViewports(1, &Viewport);
    Context->OMSetRenderTargets(1, &RTV, DSV);
}

void FRenderer::RenderFrame(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext)
{
    if (!PipelineContext.SceneView)
    {
        return;
    }

    BeginFrame(*PipelineContext.SceneView, PipelineContext.Targets);
    RunRootPipeline(RootType, PipelineContext);
    ExecutePresentPass(PipelineContext);
    EndFrame();
}

void FRenderer::EndFrame()
{
    Device.Present();
}

// ==================== Pipeline Execution ====================

void FRenderer::RunRootPipeline(ERenderPipelineType RootType, FRenderPipelineContext& PipelineContext)
{
    PreparePipelineExecution(*PipelineContext.SceneView, PipelineContext.Targets);

    // 실행 직전 최신 렌더러 상태를 PipelineContext에 다시 연결합니다.
    PipelineContext.Renderer          = this;
    PipelineContext.StateCache        = &SubmitStateCache;
    PipelineContext.Context           = Device.GetDeviceContext();
    PipelineContext.Device            = &Device;
    PipelineContext.Resources         = &FrameResources;
    PipelineContext.DrawCommandList   = &DrawCommandList;
    PipelineContext.RenderPassPresets = PassRegistry.GetRenderPassPresets();
    PipelineContext.ViewMode.Registry = ViewModePassRegistry;
    PipelineContext.LightCulling      = LightCulling.get();

    PipelineRunner.ExecutePipeline(RootType, PipelineContext, *PipelineContext.SceneView, PipelineRegistry, PassRegistry);

    FinalizePipelineExecution();
}

void FRenderer::ExecutePipeline(ERenderPipelineType Type, FRenderPipelineContext& PipelineContext)
{
    // 하위 파이프라인 실행 시에도 최신 렌더러 상태를 PipelineContext에 연결합니다.
    PipelineContext.Renderer          = this;
    PipelineContext.StateCache        = &SubmitStateCache;
    PipelineContext.Context           = Device.GetDeviceContext();
    PipelineContext.Device            = &Device;
    PipelineContext.Resources         = &FrameResources;
    PipelineContext.DrawCommandList   = &DrawCommandList;
    PipelineContext.RenderPassPresets = PassRegistry.GetRenderPassPresets();
    PipelineContext.ViewMode.Registry = ViewModePassRegistry;
    PipelineContext.LightCulling      = LightCulling.get();

    PipelineRunner.ExecutePipeline(Type, PipelineContext, *PipelineContext.SceneView, PipelineRegistry, PassRegistry);
}

void FRenderer::ExecutePresentPass(FRenderPipelineContext& PipelineContext)
{
    if (!PipelineContext.Targets || !PipelineContext.Targets->HasViewportTargets())
    {
        return;
    }

    if (FRenderPass* Pass = PassRegistry.FindPass(ERenderPassNodeType::PresentPass))
    {
        Pass->Execute(PipelineContext);
    }
}

// ==================== Pipeline Preparation Helpers ====================

void FRenderer::PreparePipelineExecution(const FSceneView& SceneView, const FViewportRenderTargets* Targets)
{
    FDrawCallStats::Reset();

    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    {
        SCOPE_STAT_CAT("UpdateFrameBuffer", "4_ExecutePass");
        UpdateFrameBuffer(Context, SceneView);
    }

    // 프레임 공용 샘플러와 draw command 정렬을 준비합니다.
    FrameResources.BindSystemSamplers(Context);
    DrawCommandList.Sort();

    // 패스 실행 중 공유할 바인딩 상태 캐시를 초기화합니다.
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

    ActiveScene                = nullptr;
    bPipelineExecutionPrepared = false;
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FSceneView& SceneView)
{
    // 프레임/뷰 공용 상수 버퍼 갱신
    FFrameCBData FrameConstantData   = {};
    FrameConstantData.View           = SceneView.View;
    FrameConstantData.Projection     = SceneView.Proj;
    FrameConstantData.InvViewProj    = (SceneView.View * SceneView.Proj).GetInverse();
    FrameConstantData.bIsWireframe   = (SceneView.ViewMode == EViewMode::Wireframe);
    FrameConstantData.WireframeColor = SceneView.WireframeColor;
    FrameConstantData.CameraWorldPos = SceneView.CameraPosition;

    if (GEngine && GEngine->GetTimer())
    {
        FrameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
    }

    FrameResources.FrameBuffer.Update(Context, &FrameConstantData, sizeof(FFrameCBData));

    ID3D11Buffer* FrameCB = FrameResources.FrameBuffer.GetBuffer();
    Context->VSSetConstantBuffers(ECBSlot::Frame, 1, &FrameCB);
    Context->PSSetConstantBuffers(ECBSlot::Frame, 1, &FrameCB);

    // 씬 라이트 상수 버퍼와 로컬 라이트 버퍼 갱신
    const FCollectedLights  EmptyLights = {};
    const FCollectedLights& Lights      = ActiveScene ? DrawCollector.GetCollectedSceneData().Lights : EmptyLights;

    FrameResources.GlobalLightBuffer.Update(Context, &Lights.GlobalLights, sizeof(FGlobalLightCBData));
    FrameResources.UpdateLocalLights(Device.GetDevice(), Context, Lights.LocalLights);
}

void FRenderer::CleanupPassState(ID3D11DeviceContext* Context, FDrawBindStateCache& Cache)
{
    // 패스에서 사용한 SRV 바인딩을 정리해 다음 프레임/패스 충돌을 방지합니다.
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

// ==================== Resource Accessors ====================

FConstantBuffer* FRenderer::AcquirePerObjectCBForProxy(const FPrimitiveProxy& Proxy)
{
    return FrameResources.GetPerObjectCBForProxy(Device.GetDevice(), Proxy);
}
