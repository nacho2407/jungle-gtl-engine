#include "Render/Execution/Renderer.h"
#include "Render/Submission/Collectors/SceneVisibilityCollector.h"

#include <iostream>
#include <algorithm>
#include <functional>
#include "Resource/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/FogParams.h"
#include "Render/Resources/Pools/ConstantBufferPool.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Render/Scene/Scene.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Engine/Runtime/Engine.h"
#include "Profiling/Timer.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Pipelines/ViewMode/ViewModePassConfig.h"
#include "Render/Resources/Frame/FrameSharedResources.h"
#include "Render/View/ViewModeSurfaceSet.h"
#include "Render/View/ViewportRenderTargets.h"
#include "Render/Execution/PipelineShaderResolver.h"
#include "Render/Passes/Common/PassRenderState.h"
#include "Render/Types/ShadingTypes.h"
#include "Materials/MaterialManager.h"


void FRenderer::Create(HWND hWindow)
{
    Device.Create(hWindow);

    if (Device.GetDevice() == nullptr)
    {
        std::cout << "Failed to create D3D Device." << std::endl;
    }

    FShaderManager::Get().Initialize(Device.GetDevice());
    FConstantBufferPool::Get().Initialize(Device.GetDevice());
    Resources.Create(Device.GetDevice());

    EditorLines.Create(Device.GetDevice());
    GridLines.Create(Device.GetDevice());
    FontGeometry.Create(Device.GetDevice());

    InitializeDefaultPassRenderStates(PassRenderStates);
    PassRegistry.Initialize();
    PipelineRegistry.Initialize();

    ViewModePassRegistry = new FViewModePassRegistry();
    ViewModePassRegistry->Initialize(Device.GetDevice());
    OwnedViewModeSurfaceSet = new FViewModeSurfaceSet();

    // GPU Profiler 초기화
    FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
    if (ViewModePassRegistry)
    {
        ViewModePassRegistry->Release();
        delete ViewModePassRegistry;
        ViewModePassRegistry = nullptr;
    }

    ClearActiveViewMode();
    ActiveViewSurfaceSet = nullptr;
    ReleaseViewModeSurfaceSet();
    if (OwnedViewModeSurfaceSet)
    {
        delete OwnedViewModeSurfaceSet;
        OwnedViewModeSurfaceSet = nullptr;
    }

    FGPUProfiler::Get().Shutdown();
    PassRegistry.Release();
    PipelineRegistry.Release();

    EditorLines.Release();
    GridLines.Release();
    FontGeometry.Release();

    for (FConstantBuffer& CB : PerObjectCBPool)
    {
        CB.Release();
    }
    PerObjectCBPool.clear();

    Resources.Release();
    FConstantBufferPool::Get().Release();
    FShaderManager::Get().Release();
    FMaterialManager::Get().Release();
    Device.Release();
}


FViewModeSurfaceSet* FRenderer::AcquireViewModeSurfaceSet(uint32 Width, uint32 Height)
{
    if (!OwnedViewModeSurfaceSet || !Device.GetDevice() || Width == 0 || Height == 0)
    {
        return nullptr;
    }

    OwnedViewModeSurfaceSet->Resize(Device.GetDevice(), Width, Height);
    ActiveViewSurfaceSet = OwnedViewModeSurfaceSet;
    return ActiveViewSurfaceSet;
}

void FRenderer::ReleaseViewModeSurfaceSet()
{
    if (OwnedViewModeSurfaceSet)
    {
        OwnedViewModeSurfaceSet->Release();
    }
    ActiveViewSurfaceSet = nullptr;
}

void FRenderer::SetActiveViewMode(EViewMode InViewMode)
{
    ActiveViewMode = InViewMode;
    bHasActiveViewModePassConfig = (ViewModePassRegistry != nullptr) && ViewModePassRegistry->HasConfig(InViewMode);
}

void FRenderer::ClearActiveViewMode()
{
    ActiveViewMode = EViewMode::Lit_Phong;
    bHasActiveViewModePassConfig = false;
}

// ============================================================
// BeginCollect — DrawCommandList + 동적 지오메트리 초기화
// Collector가 BuildCommandForProxy/AddWorldText를 호출하기 전에 반드시 호출
// ============================================================
void FRenderer::BeginCollect(const FFrameContext& Frame, uint32 MaxProxyCount)
{
    DrawCommandList.Reset();
    CollectViewMode = Frame.ViewMode;
    bHasSelectionMaskCommands = false;
    ActiveSceneForFrame = nullptr;
    ActiveCollectedLights = nullptr;

    // PerObjectCBPool 미리 할당 — Collect 도중 resize로 FDrawCommand.PerObjectCB
    // 포인터가 무효화되는 것을 방지
    if (MaxProxyCount > 0)
        EnsurePerObjectCBPoolCapacity(MaxProxyCount);

    // 동적 지오메트리 초기화
    EditorLines.Clear();
    GridLines.Clear();
    FontGeometry.ClearAll();
    FontGeometry.ClearScreen();

    if (const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default")))
        FontGeometry.EnsureCharInfoMap(FontRes);
}

//	스왑체인 백버퍼 복귀 — ImGui 합성 직전에 호출
void FRenderer::BeginFrame()
{
    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    ID3D11RenderTargetView* RTV = Device.GetFrameBufferRTV();
    ID3D11DepthStencilView* DSV = Device.GetDepthStencilView();

    Context->ClearRenderTargetView(RTV, Device.GetClearColor());
    Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

    const D3D11_VIEWPORT& Viewport = Device.GetViewport();
    Context->RSSetViewports(1, &Viewport);
    Context->OMSetRenderTargets(1, &RTV, DSV);
}

void FRenderer::PreparePipelineExecution(const FFrameContext& Frame, const FViewportRenderTargets* Targets)
{
    FDrawCallStats::Reset();

    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    {
        SCOPE_STAT_CAT("UpdateFrameBuffer", "4_ExecutePass");
        UpdateFrameBuffer(Context, Frame);
    }

    Resources.BindSystemSamplers(Context);
    DrawCommandList.Sort();

    PipelineStateCache.Reset();
    PipelineStateCache.RTV = (Targets && Targets->ViewportRTV) ? Targets->ViewportRTV : Device.GetFrameBufferRTV();
    PipelineStateCache.DSV = (Targets && Targets->ViewportDSV) ? Targets->ViewportDSV : Device.GetDepthStencilView();

    bPipelineExecutionPrepared = true;
}

void FRenderer::FinalizePipelineExecution()
{
    if (!bPipelineExecutionPrepared)
    {
        return;
    }
    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    CleanupPassState(Context, PipelineStateCache);
    ActiveSceneForFrame = nullptr;
    ActiveCollectedLights = nullptr;
    bPipelineExecutionPrepared = false;
}

// ============================================================
// CleanupPassState — 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
// ============================================================
void FRenderer::CleanupPassState(ID3D11DeviceContext* Context, FStateCache& Cache)
{
    ID3D11ShaderResourceView* NullSRVs[6] = {};
    Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);

    // 시스템 텍스처 언바인딩
    ID3D11ShaderResourceView* nullSRV = nullptr;
    Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &nullSRV);
    Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &nullSRV);
    Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &nullSRV);
    Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &nullSRV);

    Cache.Cleanup(Context);
    DrawCommandList.Reset();
}

// ============================================================
// BuildPassEvents — 패스 루프 Pre/Post 이벤트 등록
// ============================================================


// ============================================================
// PerObjectCB 풀 관리
// ============================================================

void FRenderer::EnsurePerObjectCBPoolCapacity(uint32 RequiredCount)
{
    if (PerObjectCBPool.size() >= RequiredCount)
    {
        return;
    }

    const size_t OldCount = PerObjectCBPool.size();
    PerObjectCBPool.resize(RequiredCount);

    ID3D11Device* D3DDevice = Device.GetDevice();
    for (size_t Index = OldCount; Index < PerObjectCBPool.size(); ++Index)
    {
        PerObjectCBPool[Index].Create(D3DDevice, sizeof(FPerObjectConstants));
    }
}

FConstantBuffer* FRenderer::GetPerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy)
{
    if (Proxy.ProxyId == UINT32_MAX)
    {
        return nullptr;
    }

    EnsurePerObjectCBPoolCapacity(Proxy.ProxyId + 1);
    return &PerObjectCBPool[Proxy.ProxyId];
}


//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
    Device.Present();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FFrameContext& Frame)
{
    FFrameConstants frameConstantData = {};
    frameConstantData.View = Frame.View;
    frameConstantData.Projection = Frame.Proj;
    frameConstantData.InvViewProj = (Frame.View * Frame.Proj).GetInverse();
    frameConstantData.bIsWireframe = (Frame.ViewMode == EViewMode::Wireframe);
    frameConstantData.WireframeColor = Frame.WireframeColor;
    frameConstantData.CameraWorldPos = Frame.CameraPosition;

    if (GEngine && GEngine->GetTimer())
    {
        frameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
    }

    Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
    ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
    Context->VSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
    Context->PSSetConstantBuffers(ECBSlot::Frame, 1, &b0);

    const FCollectedLights EmptyLights = {};
    const FCollectedLights& Lights = ActiveCollectedLights ? *ActiveCollectedLights : EmptyLights;

    Resources.GlobalLightBuffer.Update(Context, &Lights.GlobalLights, sizeof(FGlobalLightConstants));
    Resources.UpdateLocalLights(Device.GetDevice(), Context, Lights.LocalLights);
}


FRenderPassContext FRenderer::CreatePassContext(const FFrameContext& Frame, const FViewportRenderTargets* Targets, FScene* Scene, const TArray<FPrimitiveSceneProxy*>* VisibleProxies)
{
    FRenderPassContext PassContext = {};
    PassContext.Frame = &Frame;
    PassContext.Targets = Targets;
    PassContext.Scene = Scene ? Scene : const_cast<FScene*>(ActiveSceneForFrame);
    PassContext.Renderer = this;
    PassContext.Device = &Device;
    PassContext.Context = Device.GetDeviceContext();
    PassContext.Resources = &Resources;
    PassContext.StateCache = &PipelineStateCache;
    PassContext.DrawCommandList = &DrawCommandList;
    PassContext.PassRenderStates = PassRenderStates;
    PassContext.ViewModePassRegistry = ViewModePassRegistry;
    PassContext.ActiveViewSurfaceSet = ActiveViewSurfaceSet;
    PassContext.ActiveViewMode = ActiveViewMode;
    PassContext.CollectedPrimitives = nullptr;
    PassContext.VisibleProxies = VisibleProxies;
    if (PassContext.Scene)
    {
        PassContext.DebugLines = &PassContext.Scene->GetDebugData().GetDebugLines();
        PassContext.OverlayTexts = &PassContext.Scene->GetDebugData().GetOverlayTexts();
    }
    PassContext.Occlusion = Frame.OcclusionCulling;
    PassContext.LODContext = &Frame.LODContext;
    return PassContext;
}

FRenderPassContext FRenderer::CreatePassContext(const FFrameContext& Frame, const FViewportRenderTargets* Targets, FScene* Scene, const FCollectedPrimitives& CollectedPrimitives)
{
    FRenderPassContext PassContext = CreatePassContext(Frame, Targets, Scene, &CollectedPrimitives.VisibleProxies);
    PassContext.CollectedPrimitives = &CollectedPrimitives;
    return PassContext;
}

void FRenderer::RunRootPipeline(ERenderPipelineType RootType, FRenderPassContext& PassContext)
{
    const FFrameContext& Frame = *PassContext.Frame;
    const bool bRootToBackBuffer = !(PassContext.Targets && PassContext.Targets->ViewportRTV);
    if (bRootToBackBuffer)
    {
        BeginFrame();
    }

    PreparePipelineExecution(Frame, PassContext.Targets);
    PassContext.Renderer = this;
    PassContext.Renderer = this;
    PassContext.StateCache = &PipelineStateCache;
    PassContext.Context = Device.GetDeviceContext();
    PassContext.Device = &Device;
    PassContext.Resources = &Resources;
    PassContext.DrawCommandList = &DrawCommandList;
    PassContext.PassRenderStates = PassRenderStates;
    PassContext.ViewModePassRegistry = ViewModePassRegistry;
    PassContext.ActiveViewSurfaceSet = ActiveViewSurfaceSet;
    PassContext.ActiveViewMode = ActiveViewMode;
    PipelineRunner.ExecutePipeline(RootType, PassContext, *PassContext.Frame, PipelineRegistry, PassRegistry);
    FinalizePipelineExecution();

    if (bRootToBackBuffer)
    {
        EndFrame();
    }
}

void FRenderer::ExecutePipeline(ERenderPipelineType Type, FRenderPassContext& PassContext)
{
    const FFrameContext& Frame = *PassContext.Frame;
    PassContext.Renderer = this;
    PassContext.Renderer = this;
    PassContext.StateCache = &PipelineStateCache;
    PassContext.Context = Device.GetDeviceContext();
    PassContext.Device = &Device;
    PassContext.Resources = &Resources;
    PassContext.DrawCommandList = &DrawCommandList;
    PassContext.PassRenderStates = PassRenderStates;
    PassContext.ViewModePassRegistry = ViewModePassRegistry;
    PassContext.ActiveViewSurfaceSet = ActiveViewSurfaceSet;
    PassContext.ActiveViewMode = ActiveViewMode;
    PipelineRunner.ExecutePipeline(Type, PassContext, *PassContext.Frame, PipelineRegistry, PassRegistry);
}
