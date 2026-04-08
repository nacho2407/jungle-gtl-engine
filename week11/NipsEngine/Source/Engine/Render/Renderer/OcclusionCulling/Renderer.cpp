#include "Renderer.h"
#include <iostream>
#include <algorithm>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"

void FRenderer::Create(HWND hWindow)
{
    Device.Create(hWindow);

    if (Device.GetDevice() == nullptr)
    {
        std::cout << "Failed to create D3D Device." << std::endl;
    }

    // 1. 일반 메쉬 (Primitive.hlsl)
    Resources.PrimitiveShader.Create(Device.GetDevice(), L"Shaders/Primitive.hlsl", "VS", "PS",
                                     PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

    // 2. 기즈모 (Gizmo.hlsl)
    Resources.GizmoShader.Create(Device.GetDevice(), L"Shaders/Gizmo.hlsl", "VS", "PS",
                                 PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

    // 3. 에디터/라인 (Editor.hlsl)
    Resources.EditorShader.Create(Device.GetDevice(), L"Shaders/Editor.hlsl", "VS", "PS",
                                  PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

    // 4. 선택 마스크 (SelectionMask.hlsl)
    Resources.SelectionMaskShader.Create(Device.GetDevice(), L"Shaders/SelectionMask.hlsl", "VS",
                                         "PS", PrimitiveInputLayout,
                                         ARRAYSIZE(PrimitiveInputLayout));

    // 5. 포스트 프로세스 아웃라인 (OutlinePostProcess.hlsl)
    Resources.OutlineShader.Create(Device.GetDevice(), L"Shaders/OutlinePostProcess.hlsl", "VS",
                                   "PS", nullptr, 0);

    // 6. 스태틱 메시 (ShaderStaticMesh.hlsl)
    Resources.StaticMeshShader.Create(Device.GetDevice(), L"Shaders/StaticMeshShader.hlsl",
                                      "mainVS", "mainPS", NormalVertexInputLayout,
                                      ARRAYSIZE(NormalVertexInputLayout));

    // 7. 최적화 메시 (POSITION + TEXCOORD 전용 레이아웃 사용)
    Resources.PerformanceStaticMeshShader.Create(
        Device.GetDevice(), L"Shaders/PerformanceStaticMesh.hlsl", "mainVS", "mainPS",
        PerformanceMeshInputLayout, ARRAYSIZE(PerformanceMeshInputLayout));

    // 8. Depth Prepass (VS only — Position 전용 레이아웃)
    Resources.DepthPrepassShader.Create(Device.GetDevice(), L"Shaders/DepthPrepass.hlsl",
                                        "DepthPrepassVS", nullptr, DepthPrepassInputLayout,
                                        ARRAYSIZE(DepthPrepassInputLayout));

    // 9. HiZ Downsample CS (reverse-Z min-depth 2x2 downsample)
    Resources.HiZCopyMip0CS.Create(Device.GetDevice(), L"Shaders/HiZDownsample.hlsl",
                                   "HiZCopyMip0CS");
    Resources.HiZDownsampleCS.Create(Device.GetDevice(), L"Shaders/HiZDownsample.hlsl",
                                     "HiZDownsampleCS");
    Resources.HiZConstantBuffer.Create(Device.GetDevice(), sizeof(FHiZConstants));

    // 10. Occlusion Cull CS (GPU 가시성 테스트)
    Resources.OcclusionCullCS.Create(Device.GetDevice(), L"Shaders/OcclusionCull.hlsl",
                                     "OcclusionCullCS");
    Resources.OcclusionCullConstantBuffer.Create(Device.GetDevice(),
                                                 sizeof(FOcclusionCullConstants));

    // Occlusion AABB 버퍼 (DYNAMIC StructuredBuffer<FGPUOcclusionAABB>)
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = sizeof(FGPUOcclusionAABB) * MaxOcclusionObjects;
        Desc.Usage = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        Desc.StructureByteStride = sizeof(FGPUOcclusionAABB);
        Device.GetDevice()->CreateBuffer(&Desc, nullptr,
                                         OcclusionAABBBuffer.ReleaseAndGetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.FirstElement = 0;
        SRVDesc.Buffer.NumElements = MaxOcclusionObjects;
        Device.GetDevice()->CreateShaderResourceView(OcclusionAABBBuffer.Get(), &SRVDesc,
                                                     OcclusionAABBSRV.ReleaseAndGetAddressOf());
    }

    // Occlusion Result 버퍼 (DEFAULT RWStructuredBuffer<uint>)
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = sizeof(uint32) * MaxOcclusionObjects;
        Desc.Usage = D3D11_USAGE_DEFAULT;
        Desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        Desc.StructureByteStride = sizeof(uint32);
        Device.GetDevice()->CreateBuffer(&Desc, nullptr,
                                         OcclusionResultBuffer.ReleaseAndGetAddressOf());

        D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = MaxOcclusionObjects;
        Device.GetDevice()->CreateUnorderedAccessView(OcclusionResultBuffer.Get(), &UAVDesc,
                                                      OcclusionResultUAV.ReleaseAndGetAddressOf());
    }

    // Occlusion Result Staging 버퍼 ×2 — 더블버퍼 async readback (GPU stall 제거)
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = sizeof(uint32) * MaxOcclusionObjects;
        Desc.Usage          = D3D11_USAGE_STAGING;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        for (int s = 0; s < 2; ++s)
            Device.GetDevice()->CreateBuffer(&Desc, nullptr,
                OcclusionResultStaging[s].ReleaseAndGetAddressOf());
    }
    {
        D3D11_QUERY_DESC QueryDesc = {};
        QueryDesc.Query = D3D11_QUERY_EVENT;
        for (int s = 0; s < 2; ++s)
        {
            Device.GetDevice()->CreateQuery(&QueryDesc,
                                            OcclusionResultQueries[s].ReleaseAndGetAddressOf());
            OcclusionSubmittedVisibleIndices[s].clear();
        }
    }
    StagingWriteIdx = 0;
    OcclusionPendingSlot = 0;
    bStagingValid   = false;
    CachedOcclusionVisibleIndices.clear();
    CachedOcclusionSurvivorIndices.clear();
    bHasCachedOcclusionResults = false;
    bHasAcceptedOcclusionResults = false;
    bOcclusionReadbackPending = false;
    bHasPreviousOcclusionViewProjection = false;

    Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FPerObjectConstants));
    Resources.FrameBuffer.Create(Device.GetDevice(), sizeof(FFrameConstants));
    Resources.GizmoPerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FGizmoConstants));
    Resources.EditorConstantBuffer.Create(Device.GetDevice(), sizeof(FEditorConstants));
    Resources.OutlineConstantBuffer.Create(Device.GetDevice(), sizeof(FOutlineConstants));
    Resources.StaticMeshConstantBuffer.Create(Device.GetDevice(), sizeof(FStaticMeshConstants));
    Resources.PerformanceStaticMeshConstantBuffer.Create(Device.GetDevice(),
                                                         sizeof(FPerformanceStaticMeshConstants));
    Resources.PerObjectPerformanceBuffer.Create(Device.GetDevice(),
                                                sizeof(FPerObjectPerformanceConstants)); // b7

    // TODO : SamplerState 관리
    D3D11_SAMPLER_DESC SampDesc = {};
    SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

    SampDesc.MinLOD = 0.0f;
    SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    SampDesc.MipLODBias = 0.0f;

    Device.GetDevice()->CreateSamplerState(&SampDesc,
                                           Resources.MeshSamplerState.ReleaseAndGetAddressOf());

    //	MeshManager init
    FMeshManager::Initialize();

    EditorLineBatcher.Create(Device.GetDevice());
    GridLineBatcher.Create(Device.GetDevice());

    // 텍스처는 ResourceManager가 소유 — Batcher 는 셰이더/버퍼만 초기화
    FontBatcher.Create(Device.GetDevice());
    SubUVBatcher.Create(Device.GetDevice());

    InitializePassRenderStates();
    InitializePassBatchers();
    UseBackBufferRenderTargets();

    // GPU Profiler 초기화
    FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
    Resources.PrimitiveShader.Release();
    Resources.GizmoShader.Release();
    Resources.EditorShader.Release();
    Resources.SelectionMaskShader.Release();
    Resources.OutlineShader.Release();
    Resources.StaticMeshShader.Release();

    Resources.DepthPrepassShader.Release();
    Resources.HiZCopyMip0CS.Release();
    Resources.HiZDownsampleCS.Release();
    Resources.HiZConstantBuffer.Release();
    Resources.OcclusionCullCS.Release();
    Resources.OcclusionCullConstantBuffer.Release();
    OcclusionResultStaging[0].Reset();
    OcclusionResultStaging[1].Reset();
    OcclusionResultQueries[0].Reset();
    OcclusionResultQueries[1].Reset();
    OcclusionSubmittedVisibleIndices[0].clear();
    OcclusionSubmittedVisibleIndices[1].clear();
    CachedOcclusionVisibleIndices.clear();
    CachedOcclusionSurvivorIndices.clear();
    OcclusionPendingSlot = 0;
    bStagingValid = false;
    bHasCachedOcclusionResults = false;
    bHasAcceptedOcclusionResults = false;
    bOcclusionReadbackPending = false;
    bHasPreviousOcclusionViewProjection = false;
    OcclusionResultUAV.Reset();
    OcclusionResultBuffer.Reset();
    OcclusionAABBSRV.Reset();
    OcclusionAABBBuffer.Reset();

    Resources.PerObjectConstantBuffer.Release();
    Resources.FrameBuffer.Release();
    Resources.GizmoPerObjectConstantBuffer.Release();
    Resources.EditorConstantBuffer.Release();
    Resources.OutlineConstantBuffer.Release();
    Resources.StaticMeshConstantBuffer.Release();
    Resources.PerObjectPerformanceBuffer.Release();
    Resources.MeshSamplerState.Reset();
    Resources.PerformanceStaticMeshConstantBuffer.Release();
    Resources.MeshSamplerState.Reset();

    FGPUProfiler::Get().Shutdown();

    EditorLineBatcher.Release();
    GridLineBatcher.Release();
    FontBatcher.Release();
    SubUVBatcher.Release();

    Device.Release();
}

//	Bus → Batcher 데이터 수집 (CPU). BeginFrame 이전에 호출.
void FRenderer::PrepareBatchers(const FRenderBus& InRenderBus)
{
    for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
    {
        if (!PassBatchers[i])
            continue;

        const auto& Commands = InRenderBus.GetCommands(static_cast<ERenderPass>(i));
        const auto& AlignedCommands = GetAlignedCommands(static_cast<ERenderPass>(i), Commands);

        PassBatchers[i].Clear();
        for (const auto& Cmd : AlignedCommands)
            PassBatchers[i].Collect(Cmd, InRenderBus);
    }
}

const TArray<FRenderCommand>& FRenderer::GetAlignedCommands(ERenderPass                   Pass,
                                                            const TArray<FRenderCommand>& Commands)
{
    if (Commands.size() <= 1)
        return Commands;

    // SubUV 패스: Particle(SRV) 포인터 기준 정렬 → 같은 텍스쳐끼리 연속 배치
    if (Pass == ERenderPass::SubUV)
    {
        SortedCommandBuffer.assign(Commands.begin(), Commands.end());

        std::sort(SortedCommandBuffer.begin(), SortedCommandBuffer.end(),
                  [](const FRenderCommand& A, const FRenderCommand& B)
                  { return A.Constants.SubUV.Particle < B.Constants.SubUV.Particle; });

        return SortedCommandBuffer;
    }

    // 일반 드로우 패스: Type → MeshBuffer → DiffuseSRV 순 정렬
    // Type이 같으면 셰이더 재바인딩 없이, MeshBuffer가 같으면 VB/IB 재바인딩 없이 처리 가능
    if (Pass == ERenderPass::Opaque || Pass == ERenderPass::Translucent ||
        Pass == ERenderPass::SelectionMask || Pass == ERenderPass::DepthLess)
    {
        SortedCommandBuffer.assign(Commands.begin(), Commands.end());

        std::sort(SortedCommandBuffer.begin(), SortedCommandBuffer.end(),
                  [](const FRenderCommand& A, const FRenderCommand& B)
                  {
                      if (A.Type != B.Type)
                          return A.Type < B.Type;
                      if (A.MeshBuffer != B.MeshBuffer)
                          return A.MeshBuffer < B.MeshBuffer;
                      if (A.Type == ERenderCommandType::StaticMesh)
                      {
                          const ID3D11ShaderResourceView* SrvA =
                              A.Constants.PerformanceStaticMesh.DiffuseSRV;
                          const ID3D11ShaderResourceView* SrvB =
                              B.Constants.PerformanceStaticMesh.DiffuseSRV;
                          if (SrvA != SrvB)
                              return SrvA < SrvB;
                      }
                      return false;
                  });

        return SortedCommandBuffer;
    }

    return Commands;
}

//	GPU 프레임 시작. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
    Device.BeginFrame();
    UseBackBufferRenderTargets();

#if STATS
    FGPUProfiler::Get().BeginFrame();
#endif
}

void FRenderer::UseBackBufferRenderTargets()
{
    CurrentRenderTargets = Device.GetBackBufferRenderTargets();
    if (CurrentRenderTargets.IsValid())
    {
        ID3D11RenderTargetView* RTV = CurrentRenderTargets.SceneColorRTV;
        Device.GetDeviceContext()->OMSetRenderTargets(1, &RTV,
                                                      CurrentRenderTargets.DepthStencilView);
        Device.SetSubViewport(0, 0, static_cast<int32>(CurrentRenderTargets.Width),
                              static_cast<int32>(CurrentRenderTargets.Height));
    }
}

void FRenderer::UseViewportRenderTargets()
{
    CurrentRenderTargets = Device.GetViewportRenderTargets();
    if (!CurrentRenderTargets.IsValid())
    {
        UseBackBufferRenderTargets();
        return;
    }

    Device.SetSubViewport(0, 0, static_cast<int32>(CurrentRenderTargets.Width),
                          static_cast<int32>(CurrentRenderTargets.Height));
}

void FRenderer::Render(const FRenderDataManager& InRenderDataManager, const FRenderBus& InRenderBus)
{
    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    const TArray<FRenderCommand>& AllCommands = InRenderDataManager.GetRenderDataArray();
    const TArray<uint32>& VisibleIndices = InRenderDataManager.GetVisibleRenderCommandIndices();
    const TArray<FAABB>&  AABBs = InRenderDataManager.GetAABBBoundsArray();
    const bool bViewStableForOcclusion =
        bHasPreviousOcclusionViewProjection &&
        CachedViewProjection.Equals(PreviousOcclusionViewProjection, 1.0e-4f);
    TryConsumeOcclusionCullResult(Context, static_cast<uint32>(AllCommands.size()),
                                  !bViewStableForOcclusion);

    FFrameConstants frameConstantData = {};
    frameConstantData.ViewProjection = CachedViewProjection;
    frameConstantData.WireframeColor = FVector(1.0f, 1.0f, 1.0f);
    Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
    ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
    Context->VSSetConstantBuffers(0, 1, &b0);
    Context->PSSetConstantBuffers(0, 1, &b0);

    LastRenderCommandType = ERenderCommandType::None;
    LastMeshBuffer = nullptr;
    LastDiffuseSRV = nullptr;
    LastComponentIndex = UINT32_MAX;
    memset(&LastPerfConst, 0, sizeof(FPerformanceStaticMeshConstants));
    // ViewProjection + bIsWireframe 을 FrameBuffer(b0)에 업로드
    UpdateFrameBuffer(Context, InRenderBus);

    // Wireframe 모드일 때 래스터라이저 전환
    const ERasterizerState Rasterizer = (InRenderBus.GetViewMode() == EViewMode::Wireframe)
        ? ERasterizerState::WireFrame
        : ERasterizerState::SolidBackCull;

    Device.SetDepthStencilState(EDepthStencilState::Default);
    Device.SetBlendState(EBlendState::Opaque);
    Device.SetRasterizerState(Rasterizer);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Scene 로드 시 빌드된 WorldMatrix StructuredBuffer를 VS t1에 바인딩
    // (모든 StaticMesh 드로우 콜에서 ComponentIndex로 인덱싱하여 사용)
    ID3D11ShaderResourceView* WorldMatSRV = InRenderDataManager.GetWorldMatrixBufferSRV();
    if (WorldMatSRV)
    {
        Context->VSSetShaderResources(1, 1, &WorldMatSRV);
    }

    const bool bSameVisibleSetAsCache =
        bHasCachedOcclusionResults && (CachedOcclusionVisibleIndices == VisibleIndices);
    const bool bSameViewAsCache =
        bHasCachedOcclusionResults &&
        CachedViewProjection.Equals(CachedOcclusionResultViewProjection, 1.0e-3f);
    const bool bHasTemporalCachedResults =
        bOcclusionCullingEnabled && bHasAcceptedOcclusionResults;
    const bool bEligibleForOcclusion =
        bOcclusionCullingEnabled && VisibleIndices.size() >= MinOcclusionCullObjects;
    const bool bShouldSubmitOcclusion =
        bEligibleForOcclusion && !bOcclusionReadbackPending &&
        (!bHasAcceptedOcclusionResults || !bSameVisibleSetAsCache || !bSameViewAsCache);
    const bool bUsingCachedResults = bHasTemporalCachedResults && bEligibleForOcclusion;

    OcclusionDebugState.bEnabled = bOcclusionCullingEnabled;
    OcclusionDebugState.bEligible = bEligibleForOcclusion;
    OcclusionDebugState.bUsingCachedResults = bUsingCachedResults;
    OcclusionDebugState.bSubmittedThisFrame = false;
    OcclusionDebugState.bReadbackPending = bOcclusionReadbackPending;
    OcclusionDebugState.bHasCachedResults = bHasCachedOcclusionResults;
    if (!OcclusionDebugState.bHasReadbackResult)
    {
        OcclusionDebugState.bAcceptedCullResult = false;
        OcclusionDebugState.ProposedSurvivorCount = 0;
        OcclusionDebugState.CullSavings = 0;
        OcclusionDebugState.RequiredSavings = 0;
    }
    OcclusionDebugState.VisibleCount = static_cast<uint32>(VisibleIndices.size());
    OcclusionDebugState.CachedSurvivorCount =
        static_cast<uint32>(CachedOcclusionSurvivorIndices.size());
    OcclusionDebugState.LastSubmittedCount =
        bOcclusionReadbackPending
            ? static_cast<uint32>(OcclusionSubmittedVisibleIndices[OcclusionPendingSlot].size())
            : 0;

    TArray<uint32> SurvivorIndices;
    if (bUsingCachedResults)
    {
        BuildTemporalSurvivorIndices(VisibleIndices, static_cast<uint32>(AllCommands.size()),
                                     SurvivorIndices);
    }
    else
    {
        SurvivorIndices = VisibleIndices;
    }

    if (bShouldSubmitOcclusion)
    {
        // Depth Prepass: 프러스텀 가시 오브젝트 전체를 z-only 렌더 (정렬 불필요)
        ExecuteDepthPrepass(InRenderDataManager.GetVisibleRenderCommands(), Context);

        // Step 4: DepthPrepass -> HiZ mip0를 CS로 복사.
        // 상위 mip chain은 현재 비활성화되어 있으며 occlusion CS는 mip0만 사용한다.
        ExecuteHiZCopyMip0(Context);

        // Step 5: GPU 가시성 테스트를 제출하고, 현재 프레임은 기존 결과(없으면 frustum)로 진행
        ExecuteOcclusionCull(Context, VisibleIndices, AABBs);
        OcclusionDebugState.bSubmittedThisFrame = true;
        OcclusionDebugState.bReadbackPending = true;
        OcclusionDebugState.LastSubmittedCount = static_cast<uint32>(VisibleIndices.size());
    }
    else
    {
        if (!bOcclusionCullingEnabled)
        {
            ResetOcclusionCullState();
        }
    }

    OcclusionDebugState.CachedSurvivorCount =
        static_cast<uint32>(bUsingCachedResults ? SurvivorIndices.size()
                                                   : CachedOcclusionSurvivorIndices.size());

    // Scene 렌더 타겟 복원
    ID3D11RenderTargetView* RTV = CurrentRenderTargets.SceneColorRTV;
    Context->OMSetRenderTargets(1, &RTV, CurrentRenderTargets.DepthStencilView);

    // Step 6: survivor 커맨드 구성 → 정렬 → draw
    TArray<FRenderCommand> SurvivorCommands;
    SurvivorCommands.reserve(SurvivorIndices.size());
    for (uint32 Idx : SurvivorIndices)
        SurvivorCommands.push_back(AllCommands[Idx]);

    const TArray<FRenderCommand>& FinalCmds =
        GetAlignedCommands(ERenderPass::Opaque, SurvivorCommands);

    Device.SetDepthStencilState(EDepthStencilState::Default);
    Device.SetBlendState(EBlendState::Opaque);

    for (const auto& Cmd : FinalCmds)
    {
#if STATS
        {
            FRenderQueueStats& S = PassStats[(uint32)ERenderPass::Opaque];
            const bool         bTC = (LastRenderCommandType != Cmd.Type);
            if (bTC)
                ++S.TypeChangeCount;
            if (Cmd.MeshBuffer && Cmd.MeshBuffer != LastMeshBuffer)
                ++S.MeshRebindCount;
            if (Cmd.Type == ERenderCommandType::StaticMesh)
            {
                const auto& PC = Cmd.Constants.PerformanceStaticMesh;
                if (bTC || PC.DiffuseSRV != LastDiffuseSRV)
                    ++S.SRVRebindCount;
                if (bTC || memcmp(&PC, &LastPerfConst, 16u) != 0)
                    ++S.CBufferUpdateCount;
            }
        }
#endif
        BindShaderByType(Cmd, Context);
        DrawCommand(Context, Cmd);
    }

    PreviousOcclusionViewProjection = CachedViewProjection;
    bHasPreviousOcclusionViewProjection = true;
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FRenderBus& InRenderBus)
{
    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    UpdateFrameBuffer(Context, InRenderBus);

#if STATS
    memset(PassStats, 0, sizeof(PassStats));

    // 패스 이름 — FGPUProfiler 스냅샷 조회 시 동일 이름으로 매칭
    static const char* kPassGPUNames[(uint32)ERenderPass::MAX] = {
        "Pass_Opaque", "Pass_Font",   "Pass_SubUV",     "Pass_Translucent", "Pass_SelectionMask",
        "Pass_Grid",   "Pass_Editor", "Pass_DepthLess", "Pass_PostProcess",
    };
#endif

    for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
    {
        ERenderPass CurPass = static_cast<ERenderPass>(i);
        const auto& Commands = InRenderBus.GetCommands(CurPass);
        if (Commands.empty())
            continue;

#if STATS
        PassStats[i].CommandCount = static_cast<uint32>(Commands.size());
        uint32 GPUTimestampIdx = FGPUProfiler::Get().BeginTimestamp(kPassGPUNames[i]);
#endif

        if (PassBatchers[i])
        {
            ApplyPassRenderState(CurPass, Context, InRenderBus.GetViewMode());
            PassBatchers[i].Flush(CurPass, InRenderBus, Context);
        }
        else
        {
            const auto& SortedCmds = GetAlignedCommands(CurPass, Commands);
            ExecuteDefaultPass(CurPass, SortedCmds, InRenderBus, Context);
        }

#if STATS
        FGPUProfiler::Get().EndTimestamp(GPUTimestampIdx);
#endif
    }
}

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
    using E = ERenderPass;
    auto& S = PassRenderStates;

    //                              DepthStencil                   Blend                Rasterizer
    //                              Topology                                Shader WireframeAware
    S[(uint32)E::Opaque] = {EDepthStencilState::Default,     EBlendState::Opaque,
                            ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                            &Resources.PrimitiveShader,      true};
    S[(uint32)E::Translucent] = {
        EDepthStencilState::Default,     EBlendState::AlphaBlend,
        ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        &Resources.PrimitiveShader,      false};
    S[(uint32)E::SelectionMask] = {
        EDepthStencilState::StencilWrite, EBlendState::Opaque,
        ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        &Resources.SelectionMaskShader,   false};
    S[(uint32)E::Editor] = {EDepthStencilState::Default,     EBlendState::AlphaBlend,
                            ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
                            &Resources.EditorShader,         true};
    S[(uint32)E::Grid] = {EDepthStencilState::Default,     EBlendState::AlphaBlend,
                          ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
                          &Resources.EditorShader,         false};
    S[(uint32)E::DepthLess] = {EDepthStencilState::DepthReadOnly,
                               EBlendState::AlphaBlend,
                               ERasterizerState::SolidBackCull,
                               D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                               &Resources.GizmoShader,
                               false};
    S[(uint32)E::Font] = {EDepthStencilState::Default,
                          EBlendState::AlphaBlend,
                          ERasterizerState::SolidBackCull,
                          D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                          nullptr,
                          true};
    S[(uint32)E::SubUV] = {EDepthStencilState::Default,
                           EBlendState::AlphaBlend,
                           ERasterizerState::SolidBackCull,
                           D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                           nullptr,
                           true};
    S[(uint32)E::PostProcessOutline] = {
        EDepthStencilState::Default,   EBlendState::AlphaBlend,
        ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        &Resources.OutlineShader,      false};
}

// ============================================================
// Pass Batcher 바인딩 초기화
// ============================================================
void FRenderer::InitializePassBatchers()
{
    // --- Editor 패스: AABB 디버그 박스 → EditorLineBatcher ---
    PassBatchers[(uint32)ERenderPass::Editor] = {
        /*.Clear   =*/[this]() { EditorLineBatcher.Clear(); },
        /*.Collect =*/
        [this](const FRenderCommand& Cmd, const FRenderBus&)
        {
            if (Cmd.Type == ERenderCommandType::DebugBox)
            {
                EditorLineBatcher.AddAABB(
                    FBoundingBox{Cmd.Constants.AABB.Min, Cmd.Constants.AABB.Max},
                    Cmd.Constants.AABB.Color);
            }
        },
        /*.Flush   =*/[this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx)
        { FlushLineBatcher(EditorLineBatcher, Pass, Bus, Ctx); }};

    // --- Grid 패스: 월드 그리드 + 축 → GridLineBatcher ---
    PassBatchers[(uint32)ERenderPass::Grid] = {
        /*.Clear   =*/[this]() { GridLineBatcher.Clear(); },
        /*.Collect =*/
        [this](const FRenderCommand& Cmd, const FRenderBus& Bus)
        {
            if (Cmd.Type == ERenderCommandType::Grid)
            {
                const FVector CameraPos = Bus.GetView().GetInverse().GetOrigin();
                const FVector CameraFwd = Bus.GetCameraForward();

                GridLineBatcher.AddWorldHelpers(Bus.GetShowFlags(), Cmd.Constants.Grid.GridSpacing,
                                                Cmd.Constants.Grid.GridHalfLineCount, CameraPos,
                                                CameraFwd, Cmd.Constants.Grid.bOrthographic);
            }
        },
        /*.Flush   =*/[this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx)
        { FlushLineBatcher(GridLineBatcher, Pass, Bus, Ctx); }};

    // --- Font 패스: 텍스트 → FontBatcher ---
    PassBatchers[(uint32)ERenderPass::Font] = {
        /*.Clear   =*/[this]() { FontBatcher.Clear(); },
        /*.Collect =*/
        [this](const FRenderCommand& Cmd, const FRenderBus& Bus)
        {
            if (Cmd.Type == ERenderCommandType::Font && Cmd.Constants.Font.Text &&
                !Cmd.Constants.Font.Text->empty())
            {
                FontBatcher.AddText(
                    *Cmd.Constants.Font.Text, Cmd.PerObjectConstants.Model.GetOrigin(),
                    Bus.GetCameraRight(), Bus.GetCameraUp(),
                    Cmd.PerObjectConstants.Model.GetScaleVector(), Cmd.Constants.Font.Scale);
            }
        },
        /*.Flush   =*/
        [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx)
        {
            const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
            FontBatcher.Flush(Ctx, FontRes);
        }};

    // --- SubUV 패스: 스프라이트 → SubUVBatcher ---
    // Collect 시 첫 번째 유효한 SRV를 캡처하여 Flush에서 재순회 방지
    PassBatchers[(uint32)ERenderPass::SubUV] = {
        /*.Clear   =*/[this]()
        {
            SubUVBatcher.Clear();
            SubUVCachedSRV = nullptr;
        },
        /*.Collect =*/
        [this](const FRenderCommand& Cmd, const FRenderBus& Bus)
        {
            if (Cmd.Type == ERenderCommandType::SubUV && Cmd.Constants.SubUV.Particle)
            {
                const auto& SubUV = Cmd.Constants.SubUV;
                if (!SubUVCachedSRV && SubUV.Particle->IsLoaded())
                {
                    SubUVCachedSRV = SubUV.Particle->SRV.Get();
                }

                SubUVBatcher.AddSprite(
                    SubUV.Particle->SRV.Get(), Cmd.PerObjectConstants.Model.GetOrigin(),
                    Bus.GetCameraRight(), Bus.GetCameraUp(),
                    Cmd.PerObjectConstants.Model.GetScaleVector(), SubUV.FrameIndex,
                    SubUV.Particle->Columns, SubUV.Particle->Rows, SubUV.Width, SubUV.Height);
            }
        },
        /*.Flush   =*/[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx)
        { SubUVBatcher.Flush(Ctx); }};
}

// ============================================================
// LineBatcher Flush 공통
// ============================================================
void FRenderer::FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus,
                                 ID3D11DeviceContext* Context)
{
    if (Batcher.GetLineCount() == 0)
        return;

    const FVector    CameraPosition = Bus.GetView().GetInverse().GetOrigin();
    FEditorConstants EditorConstants = {};
    EditorConstants.CameraPosition = CameraPosition;
    Resources.EditorConstantBuffer.Update(Context, &EditorConstants, sizeof(FEditorConstants));

    ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

    ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
    Context->VSSetConstantBuffers(4, 1, &cb);
    Context->PSSetConstantBuffers(4, 1, &cb);

    Batcher.Flush(Context);
}

// ============================================================
// 기본 패스 실행기
// ============================================================
void FRenderer::ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands,
                                   const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
    // 프레임의 첫 번째 오브젝트마다 캐시 상태를 초기화
    LastRenderCommandType = ERenderCommandType::None;
    LastMeshBuffer = nullptr;
    LastDiffuseSRV = nullptr;
    memset(&LastPerfConst, 0, sizeof(FPerformanceStaticMeshConstants));

    ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

    const FPassRenderState& State = PassRenderStates[(uint32)Pass];
    for (const auto& Cmd : Commands)
    {
        EDepthStencilState TargetDepth =
            (Cmd.DepthStencilState != static_cast<EDepthStencilState>(-1)) ? Cmd.DepthStencilState
                                                                           : State.DepthStencil;

        EBlendState TargetBlend =
            (Cmd.BlendState != static_cast<EBlendState>(-1)) ? Cmd.BlendState : State.Blend;

        Device.SetDepthStencilState(TargetDepth);
        Device.SetBlendState(TargetBlend);

#if STATS
        // BindShaderByType / DrawCommand 호출 전 — Last* 캐시가 "이전 상태"를 반영할 때 카운팅
        {
            FRenderQueueStats& S = PassStats[(uint32)Pass];
            const bool         bTC = (LastRenderCommandType != Cmd.Type);
            if (bTC)
                ++S.TypeChangeCount;
            if (Cmd.MeshBuffer && Cmd.MeshBuffer != LastMeshBuffer)
                ++S.MeshRebindCount;
            if (Cmd.Type == ERenderCommandType::StaticMesh)
            {
                const auto& PC = Cmd.Constants.PerformanceStaticMesh;
                if (bTC || PC.DiffuseSRV != LastDiffuseSRV)
                    ++S.SRVRebindCount;
                if (bTC || memcmp(&PC, &LastPerfConst, 16u) != 0)
                    ++S.CBufferUpdateCount;
            }
        }
#endif

        BindShaderByType(Cmd, Context);
        if (Cmd.Type == ERenderCommandType::PostProcessOutline)
        {
            DrawPostProcessOutline(Context);
            continue;
        }
        DrawCommand(Context, Cmd);
    }
}

void FRenderer::ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context,
                                     EViewMode CurViewMode)
{
    //	Selection Mask에 대한 것인지 확인하여 RTV를 가져옴
    ID3D11RenderTargetView* RTV = (Pass == ERenderPass::SelectionMask)
                                      ? CurrentRenderTargets.SelectionMaskRTV
                                      : CurrentRenderTargets.SceneColorRTV;
    ID3D11DepthStencilView* DSV = CurrentRenderTargets.DepthStencilView;
    Context->OMSetRenderTargets(1, &RTV, DSV);

    const FPassRenderState& State = PassRenderStates[(uint32)Pass];

    ERasterizerState Rasterizer = State.Rasterizer;
    if (State.bWireframeAware && CurViewMode == EViewMode::Wireframe)
    {
        Rasterizer = ERasterizerState::WireFrame;
    }

    Device.SetDepthStencilState(State.DepthStencil);
    Device.SetBlendState(State.Blend);
    Device.SetRasterizerState(Rasterizer);
    Context->IASetPrimitiveTopology(State.Topology);

    if (State.Shader)
    {
        State.Shader->Bind(Context);
    }
}

void FRenderer::BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context)
{
    bool bTypeChanged = (LastRenderCommandType != InCmd.Type);

    // 객체별 Transform Data는 항상 업데이트해야 한다.
    // [수정됨] 무조건 PerObjectConstantBuffer를 업데이트하던 것을 StaticMesh라면 ID만 업데이트한다.
    if (InCmd.Type == ERenderCommandType::StaticMesh)
    {
        uint32 NewIndex = InCmd.PerObjectPerformanceConstants.ComponentIndex;
        if (bTypeChanged || NewIndex != LastComponentIndex)
        {
            FPerObjectPerformanceConstants PerfObjConstants;
            PerfObjConstants.ComponentIndex = NewIndex;
            Resources.PerObjectPerformanceBuffer.Update(Context, &PerfObjConstants, sizeof(FPerObjectPerformanceConstants));
            LastComponentIndex = NewIndex;
        }
    }
    else
    {
        Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));
    }

    // 데이터 Update는 항상 수행하지만, 셰이더/상수 버퍼 바인딩은 타입이 변경된 경우에만 수행
    switch (InCmd.Type)
    {
    case ERenderCommandType::Gizmo:
        Resources.GizmoPerObjectConstantBuffer.Update(Context, &InCmd.Constants.Gizmo,
                                                      sizeof(FGizmoConstants));

        if (bTypeChanged)
        {
            Resources.GizmoShader.Bind(Context);
            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            ID3D11Buffer* cb2 = Resources.GizmoPerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(2, 1, &cb2);
            Context->PSSetConstantBuffers(2, 1, &cb2);
        }
        break;

    case ERenderCommandType::SelectionMask:
        break;

    case ERenderCommandType::PostProcessOutline:
    {
        FOutlineConstants outlineConstants = InCmd.Constants.Outline;
        outlineConstants.ViewportSize =
            FVector2(CurrentRenderTargets.Width, CurrentRenderTargets.Height);

        Resources.OutlineShader.Bind(Context);
        Resources.OutlineConstantBuffer.Update(Context, &outlineConstants,
                                               sizeof(FOutlineConstants));
        ID3D11Buffer* cb = Resources.OutlineConstantBuffer.GetBuffer();
        Context->VSSetConstantBuffers(5, 1, &cb);
        Context->PSSetConstantBuffers(5, 1, &cb);
        break;
    }

    case ERenderCommandType::StaticMesh:
    {
        const FPerformanceStaticMeshConstants& PerfConst = InCmd.Constants.PerformanceStaticMesh;

        // 1. 타입이 바뀌어 셰이더를 새로 바인딩해야 할 때 (최초 1회 또는 다른 타입에서 넘어왔을 때)
        if (bTypeChanged)
        {
            Resources.PerformanceStaticMeshShader.Bind(Context);

            // ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            // Context->VSSetConstantBuffers(1, 1, &cb1);

            // b7 : ComponentIndex cbuffer → VS에서 WorldMatrices 인덱스 조회에 사용
            ID3D11Buffer* cb7 = Resources.PerObjectPerformanceBuffer.GetBuffer();
            Context->VSSetConstantBuffers(7, 1, &cb7);

            ID3D11Buffer* cb6 = Resources.PerformanceStaticMeshConstantBuffer.GetBuffer();
            Context->PSSetConstantBuffers(6, 1, &cb6);

            // 샘플러 상태는 한 번만 바인딩
            ID3D11SamplerState* Samplers[] = {Resources.MeshSamplerState.Get()};
            Context->PSSetSamplers(0, 1, Samplers);
        }

        // 2. 머터리얼 데이터(색상 등)가 바뀌었을 때만 cbuffer Update (memcmp로 비교)
        if (bTypeChanged || memcmp(&PerfConst, &LastPerfConst, 16u) != 0)
        {
            Resources.PerformanceStaticMeshConstantBuffer.Update(Context, &PerfConst, 16u);
            memcpy(&LastPerfConst, &PerfConst, 16u);
        }

        // 3. 텍스처(SRV)가 바뀌었을 때만 파이프라인에 바인딩
        if (bTypeChanged || PerfConst.DiffuseSRV != LastDiffuseSRV)
        {
            ID3D11ShaderResourceView* SRVs[1] = {PerfConst.DiffuseSRV};
            Context->PSSetShaderResources(0, 1, SRVs);

            LastDiffuseSRV = PerfConst.DiffuseSRV;
        }

        break;
    }
    }

    LastRenderCommandType = InCmd.Type;
}

void FRenderer::DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand)
{
    if (InCommand.MeshBuffer == nullptr || !InCommand.MeshBuffer->IsValid())
    {
        return;
    }

	// LOD 전환으로 비활성화된 섹션 스킵
	if (InCommand.SectionIndexCount == 0)
	{
		return;
	}

	ID3D11Buffer* vertexBuffer = InCommand.MeshBuffer->GetVertexBuffer().GetBuffer();
	if (vertexBuffer == nullptr)
	{
		return;
	}

    uint32 vertexCount = InCommand.MeshBuffer->GetVertexBuffer().GetVertexCount();
    uint32 stride = InCommand.MeshBuffer->GetVertexBuffer().GetStride();
    if (vertexCount == 0 || stride == 0)
    {
        return;
    }

    // MeshBuffer가 바뀐 경우에만 VB/IB 재바인딩
    if (InCommand.MeshBuffer != LastMeshBuffer)
    {
        uint32 offset = 0;
        InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

        ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
        if (indexBuffer != nullptr)
        {
            InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        }

        LastMeshBuffer = InCommand.MeshBuffer;
    }

    ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
    if (indexBuffer != nullptr)
    {
        InDeviceContext->DrawIndexed(InCommand.SectionIndexCount, InCommand.SectionIndexStart, 0);
    }
    else
    {
        InDeviceContext->Draw(vertexCount, 0);
    }
}

void FRenderer::DrawPostProcessOutline(ID3D11DeviceContext* InDeviceContext)
{
    ID3D11RenderTargetView* RTV = CurrentRenderTargets.SceneColorRTV;
    InDeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
    InDeviceContext->OMSetDepthStencilState(nullptr, 0);

    ID3D11ShaderResourceView* maskSRV = CurrentRenderTargets.SelectionMaskSRV;
    InDeviceContext->PSSetShaderResources(7, 1, &maskSRV);

    InDeviceContext->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    InDeviceContext->PSSetShaderResources(7, 1, &nullSRV);
}

void FRenderer::SetCurrentViewProjection(const FMatrix& InVP) { CachedViewProjection = InVP; }

void FRenderer::SetCurrentOcclusionCameraPosition(const FVector& InPosition)
{
    CachedOcclusionCameraPosition = InPosition;
}

void FRenderer::SetOcclusionCullingEnabled(bool bEnabled)
{
    if (bOcclusionCullingEnabled == bEnabled)
    {
        return;
    }

    bOcclusionCullingEnabled = bEnabled;
    ResetOcclusionCullState();
}

void FRenderer::SetCurrentOcclusionCameraBasis(const FVector& InForward, const FVector& InRight,
                                               const FVector& InUp)
{
    CachedOcclusionCameraForward = InForward.GetSafeNormal();
    CachedOcclusionCameraRight = InRight.GetSafeNormal();
    CachedOcclusionCameraUp = InUp.GetSafeNormal();
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
#if STATS
    FGPUProfiler::Get().EndFrame();
#endif
    Device.EndFrame();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
    FFrameConstants frameConstantData;
    frameConstantData.ViewProjection = InRenderBus.GetView() * InRenderBus.GetProj();
    frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
    frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();

    CachedViewProjection = frameConstantData.ViewProjection;

    Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
    ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
    Context->VSSetConstantBuffers(0, 1, &b0);
    Context->PSSetConstantBuffers(0, 1, &b0);
}
