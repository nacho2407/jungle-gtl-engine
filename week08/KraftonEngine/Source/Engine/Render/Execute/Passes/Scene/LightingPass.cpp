#include "Render/Execute/Passes/Scene/LightingPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Visibility/LightCulling/TileBasedLightCulling.h"
#include "Profiling/Stats.h"

namespace
{
void BuildSurfaceSRVTable(const FRenderPipelineContext& Context, EShadingModel ShadingModel, ID3D11ShaderResourceView* OutSurfaceSRVs[6])
{
    for (uint32 i = 0; i < 6; ++i)
    {
        OutSurfaceSRVs[i] = nullptr;
    }

    if (!Context.ViewMode.Surfaces)
    {
        return;
    }

    OutSurfaceSRVs[0] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::BaseColor);
    OutSurfaceSRVs[3] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::ModifiedBaseColor);

    switch (ShadingModel)
    {
    case EShadingModel::Gouraud:
        OutSurfaceSRVs[1] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface1);
        break;

    case EShadingModel::Lambert:
        OutSurfaceSRVs[1] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface1);
        OutSurfaceSRVs[4] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::ModifiedSurface1);
        break;

    case EShadingModel::BlinnPhong:
        // Surface1 = Normal, Surface2 = MaterialParam
        OutSurfaceSRVs[1] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface1);
        OutSurfaceSRVs[2] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::Surface2);
        OutSurfaceSRVs[4] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::ModifiedSurface1);
        OutSurfaceSRVs[5] = Context.ViewMode.Surfaces->GetSRV(EViewModeSurfaceslot::ModifiedSurface2);
        break;

    case EShadingModel::Unlit:
    default:
        break;
    }
}
} // namespace

bool FLightingPass::IsEnabled(const FRenderPipelineContext& Context) const
{
    if (!Context.SceneView)
    {
        return false;
    }

    switch (Context.SceneView->ViewMode)
    {
    case EViewMode::Unlit:
    case EViewMode::WorldNormal:
    case EViewMode::Wireframe:
    case EViewMode::SceneDepth:
        return false;
    default:
        break;
    }

    return Context.ViewMode.Registry && Context.ViewMode.Registry->UsesLightingPass(Context.ViewMode.ActiveViewMode);
}

static bool SupportsLightCullStats(EViewMode ViewMode)
{
    switch (ViewMode)
    {
    case EViewMode::Lit_Gouraud:
    case EViewMode::Unlit:
    case EViewMode::WorldNormal:
    case EViewMode::Wireframe:
    case EViewMode::SceneDepth:
        return false;
    default:
        return true;
    }
}

void FLightingPass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.ViewMode.Surfaces || !Context.ViewMode.Registry || !Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        return;
    }

    const EShadingModel ShadingModel = Context.ViewMode.Registry->GetShadingModel(Context.ViewMode.ActiveViewMode);
    if (ShadingModel == EShadingModel::Unlit)
    {
        return;
    }

    const bool bNeedsReadableDepth = Targets && Targets->DepthTexture && Targets->DepthCopyTexture &&
                                     Targets->DepthTexture != Targets->DepthCopyTexture;

    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    if (bNeedsReadableDepth)
    {
        Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
    }

    ID3D11ShaderResourceView* SurfaceSRVs[6] = {};
    BuildSurfaceSRVTable(Context, ShadingModel, SurfaceSRVs);
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(SurfaceSRVs), SurfaceSRVs);

    if (Context.LightCulling)
    {
        ID3D11ShaderResourceView* TileMaskSRV = Context.LightCulling->GetPerTileMaskSRV();
        Context.Context->PSSetShaderResources(7, 1, &TileMaskSRV);

        ID3D11ShaderResourceView* HipMapSRV = Context.LightCulling->GetDebugHitMapSRV();
        Context.Context->PSSetShaderResources(8, 1, &HipMapSRV);

        // b2 LightCullingParams
        ID3D11Buffer* LightCullingParamsCB = Context.LightCulling->GetLightCullingParamsCB();
        Context.Context->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &LightCullingParamsCB);
    }

    if (Targets && Targets->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    if (Context.Resources)
    {
        ID3D11Buffer* GlobalLightBuffer = Context.Resources->GlobalLightBuffer.GetBuffer();
        Context.Context->PSSetConstantBuffers(ECBSlot::Light, 1, &GlobalLightBuffer);

        ID3D11ShaderResourceView* LocalLightsSRV = Context.Resources->LocalLightSRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &LocalLightsSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->LightCB       = Context.Resources ? &Context.Resources->GlobalLightBuffer : nullptr;
        Context.StateCache->LocalLightSRV = Context.Resources ? Context.Resources->LocalLightSRV : nullptr;
        Context.StateCache->DiffuseSRV    = nullptr;
        Context.StateCache->NormalSRV     = nullptr;
        Context.StateCache->SpecularSRV   = nullptr;
        Context.StateCache->bForceAll     = true;
    }
}

void FLightingPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FLightingPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.ViewMode.Surfaces || !Context.ViewMode.Registry || !Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        return;
    }

    if (Context.ViewMode.Registry->GetShadingModel(Context.ViewMode.ActiveViewMode) == EShadingModel::Unlit)
    {
        return;
    }

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::Lighting, Context, *Context.DrawCommandList);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FDrawCommand& Command  = Context.DrawCommandList->GetCommands().back();
    Command.PerShaderCB[0] = Context.LightCulling ? Context.LightCulling->GetLightCullingParamsCBWrapper() : nullptr;
}

void FLightingPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.DrawCommandList)
    {
        return;
    }

    const bool bMeasureLightCullStats =
        FLightCullStats::IsEnabled() &&
        Context.SceneView &&
        SupportsLightCullStats(Context.SceneView->ViewMode);

    // ---- ⏱️ 1. 쿼리 지연 초기화 (최초 1회만 실행) ----
    if (bMeasureLightCullStats && !bQueryInitialized && Context.Context)
    {
        ID3D11Device* device = nullptr;
        Context.Context->GetDevice(&device); // DeviceContext에서 Device를 얻어옴

        if (device)
        {
            D3D11_QUERY_DESC queryDesc;
            queryDesc.Query     = D3D11_QUERY_TIMESTAMP_DISJOINT;
            queryDesc.MiscFlags = 0;
            HRESULT disjointHr  = device->CreateQuery(&queryDesc, DisjointQuery.GetAddressOf());

            queryDesc.Query = D3D11_QUERY_TIMESTAMP;
            HRESULT startHr = device->CreateQuery(&queryDesc, TimestampStartQuery.GetAddressOf());
            HRESULT endHr   = device->CreateQuery(&queryDesc, TimestampEndQuery.GetAddressOf());

            // 💡 1. 카운터 버퍼(UAV용) 생성 (Structured 아님!)
            D3D11_BUFFER_DESC bufDesc = {};
            bufDesc.ByteWidth         = 16; // 4바이트 uint가 4개 들어가는 16바이트로 넉넉하게
            bufDesc.Usage             = D3D11_USAGE_DEFAULT;
            bufDesc.BindFlags         = D3D11_BIND_UNORDERED_ACCESS;
            bufDesc.MiscFlags         = 0; // 🚨 D3D11_RESOURCE_MISC_BUFFER_STRUCTURED 제거!
            HRESULT counterHr         = device->CreateBuffer(&bufDesc, nullptr, EvalCounterBuffer.GetAddressOf());

            // 💡 2. UAV 생성 (확실한 타입 명시!)
            D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format                           = DXGI_FORMAT_R32_UINT; // 🚨 UNKNOWN 대신 R32_UINT 사용
            uavDesc.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.NumElements               = 4;
            HRESULT uavHr                            = SUCCEEDED(counterHr)
                                                           ? device->CreateUnorderedAccessView(EvalCounterBuffer.Get(), &uavDesc, EvalCounterUAV.GetAddressOf())
                                                           : counterHr;

            // 3. Staging 버퍼(CPU Read용) 생성 (기존과 동일)
            bufDesc.Usage          = D3D11_USAGE_STAGING;
            bufDesc.BindFlags      = 0;
            bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            HRESULT stagingHr      = device->CreateBuffer(&bufDesc, nullptr, EvalStagingBuffer.GetAddressOf());

            device->Release(); // GetDevice는 참조 카운트를 올리므로 꼭 Release() 해줘야 메모리 누수가 안 생깁니다.
            bQueryInitialized =
                SUCCEEDED(disjointHr) &&
                SUCCEEDED(startHr) &&
                SUCCEEDED(endHr) &&
                SUCCEEDED(counterHr) &&
                SUCCEEDED(uavHr) &&
                SUCCEEDED(stagingHr);
        }
    }

    // ---- ⏱️ 2. GPU 타이밍 측정 시작 ----
    if (bMeasureLightCullStats && bQueryInitialized)
    {
        Context.Context->Begin(DisjointQuery.Get());
        Context.Context->End(TimestampStartQuery.Get());

        // 💡 [추가] UAV를 0으로 초기화
        UINT clearVals[4] = { 0, 0, 0, 0 };
        Context.Context->ClearUnorderedAccessViewUint(EvalCounterUAV.Get(), clearVals); // 기존에 묶인 RTV를 가져와서 UAV와 함께 다시 묶음
        ID3D11RenderTargetView* currentRTVs[1] = { nullptr };
        ID3D11DepthStencilView* currentDSV     = nullptr;
        Context.Context->OMGetRenderTargets(1, currentRTVs, &currentDSV);

        // RTV는 슬롯 0, UAV는 슬롯 1 (StartSlot = 1)에 바인딩
        UINT                       uavInitialCounts = 0;
        ID3D11UnorderedAccessView* pUAV             = EvalCounterUAV.Get();
        Context.Context->OMSetRenderTargetsAndUnorderedAccessViews(
            1, currentRTVs, currentDSV, 1, 1, &pUAV, &uavInitialCounts);

        // OMGetRenderTargets는 참조 카운트를 올리므로 반드시 풀어줘야 메모리 누수가 안 생깁니다.
        if (currentRTVs[0])
            currentRTVs[0]->Release();
        if (currentDSV)
            currentDSV->Release();
    }

    // 진짜 조명 연산 제출
    SubmitPassRange(Context, ERenderPass::Lighting);

    if (bMeasureLightCullStats && bQueryInitialized)
    {
        ID3D11RenderTargetView* currentRTVs[1] = { nullptr };
        ID3D11DepthStencilView* currentDSV     = nullptr;
        Context.Context->OMGetRenderTargets(1, currentRTVs, &currentDSV);

        // UAV 슬롯을 명시하지 않고 RTV만 다시 세팅하면, 꽂혀있던 UAV가 자동으로 뽑힙니다.
        Context.Context->OMSetRenderTargets(1, currentRTVs, currentDSV);

        if (currentRTVs[0])
            currentRTVs[0]->Release();
        if (currentDSV)
            currentDSV->Release();
    }

    if (bMeasureLightCullStats && bQueryInitialized)
    {
        Context.Context->End(TimestampEndQuery.Get());
        Context.Context->End(DisjointQuery.Get());

        // 주의: 이 루프는 GPU가 연산을 마칠 때까지 CPU를 멈춰 세웁니다 (성능 측정용으로만 사용)
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
        while (Context.Context->GetData(DisjointQuery.Get(), &disjointData, sizeof(disjointData), 0) == S_FALSE)
        {
        }

        bool bHasValidGPUTime = false;
        if (disjointData.Disjoint == FALSE)
        {
            UINT64 startTime = 0;
            while (Context.Context->GetData(TimestampStartQuery.Get(), &startTime, sizeof(startTime), 0) == S_FALSE)
            {
            }

            UINT64 endTime = 0;
            while (Context.Context->GetData(TimestampEndQuery.Get(), &endTime, sizeof(endTime), 0) == S_FALSE)
            {
            }

            LastGPUTimeMs    = float(endTime - startTime) / float(disjointData.Frequency) * 1000.0f;
            bHasValidGPUTime = true;
        }

        // 💡 [추가] GPU의 버퍼 값을 CPU가 읽을 수 있는 Staging 버퍼로 복사
        Context.Context->CopyResource(EvalStagingBuffer.Get(), EvalCounterBuffer.Get());

        // 💡 [추가] 메모리 맵핑을 통해 값 읽어오기
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(Context.Context->Map(EvalStagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        {
            uint32_t evaluations = *((uint32_t*)mapped.pData);
            Context.Context->Unmap(EvalStagingBuffer.Get(), 0);

            FLightCullStats::Record(bHasValidGPUTime ? LastGPUTimeMs : 0.0f, evaluations);
        }
    }

    if (Targets && Targets->ViewportRenderTexture && Targets->SceneColorCopyTexture &&
        Targets->ViewportRenderTexture != Targets->SceneColorCopyTexture)
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);

        BindViewportTarget(Context);
    }

    ID3D11ShaderResourceView* nullSRV = {};
    Context.Context->PSSetShaderResources(7, 1, &nullSRV);

    Context.Context->PSSetShaderResources(8, 1, &nullSRV);
}
