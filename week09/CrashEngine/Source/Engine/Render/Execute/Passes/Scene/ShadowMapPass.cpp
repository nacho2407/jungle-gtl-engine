#include "ShadowMapPass.h"

#include "Component/LightComponent.h"
#include "Core/Logging/LogMacros.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shadows/ShadowMapSettings.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"
#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Profiling/Stats.h"

namespace
{
const char* GetLightTypeName(uint32 LightType)
{
    switch (static_cast<ELightType>(LightType))
    {
    case ELightType::Ambient:
        return "Ambient";
    case ELightType::Directional:
        return "Directional";
    case ELightType::Point:
        return "Point";
    case ELightType::Spot:
        return "Spot";
    default:
        return "Unknown";
    }
}

FString GetLightLabel(const FLightProxy* Light)
{
    return (Light && Light->Owner) ? Light->Owner->GetFName().ToString() : FString("UnnamedLight");
}

FString FormatShadowAllocation(const FShadowMapData* Allocation)
{
    if (!Allocation)
    {
        return FString("Unallocated");
    }

    return "page=" + std::to_string(Allocation->AtlasPageIndex) +
           " slice=" + std::to_string(Allocation->SliceIndex) +
           " rect=(" + std::to_string(Allocation->ViewportRect.X) +
           ", " + std::to_string(Allocation->ViewportRect.Y) +
           " " + std::to_string(Allocation->ViewportRect.Width) +
           "x" + std::to_string(Allocation->ViewportRect.Height) + ")";
}

struct FShadowDebugPreviewCBData
{
    FMatrix InvViewProj = FMatrix::Identity;
    uint32  ShadowDepthPreviewMode = static_cast<uint32>(EShadowDepthPreviewMode::LinearizedDepth);
    float   Padding[3] = {};
};
} // namespace

FShadowMapPass::~FShadowMapPass()
{
    ReleaseShadowAtlasResources();
}

void FShadowMapPass::BeginShadowFrame()
{
    ShadowAllocationMap.BeginFrame();
}

void FShadowMapPass::EndShadowFrame()
{
    ShadowAllocationMap.EndFrame(AtlasPool);
}

bool FShadowMapPass::UpdateLightShadowAllocation(FLightProxy& Light, ID3D11Device* Device)
{
    return ShadowAllocationMap.UpdateLightShadow(Light, Device, AtlasPool);
}

void FShadowMapPass::ReleaseShadowAtlasResources()
{
    ShadowAllocationMap.Release(AtlasPool);
    AtlasPool.Release();
    MomentFilter.Release();
    ReleaseDebugPreviewResources();
    RenderItems.clear();
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowAtlasSRV(uint32 PageIndex) const
{
    const FShadowAtlasPage* Page = AtlasPool.GetPage(PageIndex);
    return Page ? Page->GetDepthArraySRV() : nullptr;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowMomentSRV(uint32 PageIndex) const
{
    const FShadowAtlasPage* Page = AtlasPool.GetPage(PageIndex);
    return Page ? Page->GetMomentArraySRV() : nullptr;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowPreviewSRV(const FShadowMapData& ShadowMapData) const
{
    if (!ShadowMapData.bAllocated)
    {
        return nullptr;
    }

    const FShadowAtlasPage* Page = AtlasPool.GetPage(ShadowMapData.AtlasPageIndex);
    return Page ? Page->GetPreviewSliceSRV(ShadowMapData.SliceIndex) : nullptr;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowMomentPreviewSRV(const FShadowMapData& ShadowMapData) const
{
    if (!ShadowMapData.bAllocated)
    {
        return nullptr;
    }

    const FShadowAtlasPage* Page = AtlasPool.GetPage(ShadowMapData.AtlasPageIndex);
    return Page ? Page->GetMomentSliceSRV(ShadowMapData.SliceIndex) : nullptr;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowPageSlicePreviewSRV(uint32 PageIndex, uint32 SliceIndex) const
{
    const FShadowAtlasPage* Page = AtlasPool.GetPage(PageIndex);
    return Page ? Page->GetPreviewSliceSRV(SliceIndex) : nullptr;
}

void FShadowMapPass::GetShadowPageSliceAllocations(uint32 PageIndex, uint32 SliceIndex, TArray<FShadowMapData>& OutAllocations) const
{
    OutAllocations.clear();

    const FShadowAtlasPage* Page = AtlasPool.GetPage(PageIndex);
    if (!Page)
    {
        return;
    }

    Page->GatherSliceAllocations(SliceIndex, PageIndex, OutAllocations);
}

uint32 FShadowMapPass::GetShadowAtlasPageCount() const
{
    return AtlasPool.GetPageCount();
}

void FShadowMapPass::EnsureDebugPreviewResources(ID3D11Device* Device)
{
    if (Device == nullptr)
    {
        return;
    }

    if (DebugPreview.VS == nullptr || DebugPreview.PS == nullptr)
    {
        FShaderStageDesc PreviewVSDesc = {};
        PreviewVSDesc.FilePath         = "Shaders/Passes/Scene/Shared/ShadowDepthPreviewPass.hlsl";
        PreviewVSDesc.EntryPoint       = "VS";

        FShaderStageDesc PreviewPSDesc = {};
        PreviewPSDesc.FilePath         = "Shaders/Passes/Scene/Shared/ShadowDepthPreviewPass.hlsl";
        PreviewPSDesc.EntryPoint       = "PS";

        ID3DBlob* VsBlob = nullptr;
        ID3DBlob* PsBlob = nullptr;
        const bool bCompiledVS = FShaderProgramBase::CompileShaderBlobStandalone(
            &VsBlob, PreviewVSDesc, "vs_5_0", "Shadow Debug Preview VS Compile Error");
        const bool bCompiledPS = FShaderProgramBase::CompileShaderBlobStandalone(
            &PsBlob, PreviewPSDesc, "ps_5_0", "Shadow Debug Preview PS Compile Error");
        if (!bCompiledVS || !bCompiledPS)
        {
            UE_LOG(Render, Error, "Failed to compile shadow debug preview shaders.");
            if (VsBlob) VsBlob->Release();
            if (PsBlob) PsBlob->Release();
            return;
        }

        const HRESULT HrVS = Device->CreateVertexShader(VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), nullptr, &DebugPreview.VS);
        const HRESULT HrPS = Device->CreatePixelShader(PsBlob->GetBufferPointer(), PsBlob->GetBufferSize(), nullptr, &DebugPreview.PS);

        VsBlob->Release();
        PsBlob->Release();

        if (FAILED(HrVS) || FAILED(HrPS))
        {
            UE_LOG(Render, Error, "Failed to create shadow debug preview shader objects.");
            ReleaseDebugPreviewResources();
            return;
        }

        UE_LOG(Render, Verbose, "Initialized shadow debug preview shaders.");
    }

    if (DebugPreview.CB == nullptr)
    {
        D3D11_BUFFER_DESC CbDesc = {};
        CbDesc.ByteWidth = sizeof(FShadowDebugPreviewCBData);
        CbDesc.Usage = D3D11_USAGE_DYNAMIC;
        CbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        CbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&CbDesc, nullptr, &DebugPreview.CB)))
        {
            UE_LOG(Render, Error, "Failed to create shadow debug preview constant buffer.");
            return;
        }
    }

    if (DebugPreview.Texture && DebugPreview.RTV && DebugPreview.SRV)
    {
        return;
    }

    D3D11_TEXTURE2D_DESC PreviewDesc = {};
    PreviewDesc.Width = DebugPreview.Size;
    PreviewDesc.Height = DebugPreview.Size;
    PreviewDesc.MipLevels = 1;
    PreviewDesc.ArraySize = 1;
    PreviewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    PreviewDesc.SampleDesc.Count = 1;
    PreviewDesc.Usage = D3D11_USAGE_DEFAULT;
    PreviewDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(Device->CreateTexture2D(&PreviewDesc, nullptr, &DebugPreview.Texture)) ||
        FAILED(Device->CreateRenderTargetView(DebugPreview.Texture, nullptr, &DebugPreview.RTV)) ||
        FAILED(Device->CreateShaderResourceView(DebugPreview.Texture, nullptr, &DebugPreview.SRV)))
    {
        UE_LOG(Render, Error, "Failed to create shadow debug preview render target resources.");
        ReleaseDebugPreviewResources();
    }
    else
    {
        UE_LOG(Render, Verbose, "Created shadow debug preview render target. Size=%u", DebugPreview.Size);
    }
}

void FShadowMapPass::ReleaseDebugPreviewResources()
{
    if (DebugPreview.SRV) { DebugPreview.SRV->Release(); DebugPreview.SRV = nullptr; }
    if (DebugPreview.RTV) { DebugPreview.RTV->Release(); DebugPreview.RTV = nullptr; }
    if (DebugPreview.Texture) { DebugPreview.Texture->Release(); DebugPreview.Texture = nullptr; }
    if (DebugPreview.CB) { DebugPreview.CB->Release(); DebugPreview.CB = nullptr; }
    if (DebugPreview.PS) { DebugPreview.PS->Release(); DebugPreview.PS = nullptr; }
    if (DebugPreview.VS) { DebugPreview.VS->Release(); DebugPreview.VS = nullptr; }
}

bool FShadowMapPass::HasPSMCameraChanged(const FSceneView& SceneView)
{
    const bool bChanged =
        !PSMCameraState.bHasLastCamera ||
        FVector::DistSquared(PSMCameraState.LastPosition, SceneView.CameraPosition) > 1e-4f ||
        FVector::DistSquared(PSMCameraState.LastForward, SceneView.CameraForward) > 1e-6f ||
        FVector::DistSquared(PSMCameraState.LastUp, SceneView.CameraUp) > 1e-6f;

    PSMCameraState.LastPosition = SceneView.CameraPosition;
    PSMCameraState.LastForward = SceneView.CameraForward;
    PSMCameraState.LastUp = SceneView.CameraUp;
    PSMCameraState.bHasLastCamera = true;
    return bChanged;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowDebugPreviewSRV(
    const FShadowMapData& ShadowMapData,
    const FMatrix&        ViewProj,
    EShadowDepthPreviewMode ShadowDepthPreviewMode,
    ID3D11Device*         Device,
    ID3D11DeviceContext*  DeviceContext)
{
    if (!ShadowMapData.bAllocated || Device == nullptr || DeviceContext == nullptr)
    {
        return nullptr;
    }

    EnsureDebugPreviewResources(Device);
    if (!DebugPreview.VS || !DebugPreview.PS || !DebugPreview.CB || !DebugPreview.RTV || !DebugPreview.SRV)
    {
        return nullptr;
    }

    ID3D11ShaderResourceView* SourceSRV = GetShadowPreviewSRV(ShadowMapData);
    if (!SourceSRV)
    {
        return nullptr;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(DeviceContext->Map(DebugPreview.CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        FShadowDebugPreviewCBData PreviewCBData = {};
        PreviewCBData.InvViewProj = ViewProj.GetInverse();
        PreviewCBData.ShadowDepthPreviewMode = static_cast<uint32>(ShadowDepthPreviewMode);
        std::memcpy(Mapped.pData, &PreviewCBData, sizeof(PreviewCBData));
        DeviceContext->Unmap(DebugPreview.CB, 0);
    }

    ID3D11RenderTargetView* SavedRTV = nullptr;
    ID3D11DepthStencilView* SavedDSV = nullptr;
    DeviceContext->OMGetRenderTargets(1, &SavedRTV, &SavedDSV);

    D3D11_VIEWPORT SavedViewport = {};
    uint32 NumViewports = 1;
    DeviceContext->RSGetViewports(&NumViewports, &SavedViewport);

    D3D11_RECT SavedScissor = {};
    uint32 NumScissors = 1;
    DeviceContext->RSGetScissorRects(&NumScissors, &SavedScissor);

    const float ClearColor[4] = { 0.03f, 0.03f, 0.03f, 1.0f };
    DeviceContext->ClearRenderTargetView(DebugPreview.RTV, ClearColor);

    D3D11_VIEWPORT PreviewViewport = {};
    PreviewViewport.TopLeftX = 0.0f;
    PreviewViewport.TopLeftY = 0.0f;
    PreviewViewport.Width = static_cast<float>(DebugPreview.Size);
    PreviewViewport.Height = static_cast<float>(DebugPreview.Size);
    PreviewViewport.MinDepth = 0.0f;
    PreviewViewport.MaxDepth = 1.0f;
    DeviceContext->RSSetViewports(1, &PreviewViewport);

    D3D11_RECT PreviewScissor = {};
    PreviewScissor.left = 0;
    PreviewScissor.top = 0;
    PreviewScissor.right = static_cast<LONG>(DebugPreview.Size);
    PreviewScissor.bottom = static_cast<LONG>(DebugPreview.Size);
    DeviceContext->RSSetScissorRects(1, &PreviewScissor);

    DeviceContext->IASetInputLayout(nullptr);
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->VSSetShader(DebugPreview.VS, nullptr, 0);
    DeviceContext->PSSetShader(DebugPreview.PS, nullptr, 0);
    DeviceContext->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &DebugPreview.CB);
    DeviceContext->OMSetRenderTargets(1, &DebugPreview.RTV, nullptr);
    DeviceContext->PSSetShaderResources(0, 1, &SourceSRV);
    DeviceContext->Draw(3, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    DeviceContext->PSSetShaderResources(0, 1, &NullSRV);

    DeviceContext->RSSetViewports(1, &SavedViewport);
    DeviceContext->RSSetScissorRects(1, &SavedScissor);
    DeviceContext->OMSetRenderTargets(1, &SavedRTV, SavedDSV);
    if (SavedRTV) SavedRTV->Release();
    if (SavedDSV) SavedDSV->Release();

    return DebugPreview.SRV;
}

void FShadowMapPass::PrepareInputs(FRenderPipelineContext& Context)
{
    ID3D11ShaderResourceView* NullSRVs[6] = {};
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(NullSRVs), NullSRVs);
}

void FShadowMapPass::PrepareTargets(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FShadowMapPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    RenderItems.clear();
PSMCameraState.bLoggedRedrawThisFrame = GetShadowMapMethod() == EShadowMapMethod::PSM &&
                                            Context.SceneView != nullptr &&
                                            HasPSMCameraChanged(*Context.SceneView);

    auto AppendRenderItem = [&](FLightProxy* Light, const FShadowMapData* Allocation, const FShadowViewData& ShadowView)
    {
        if (!Light || !Allocation || !Allocation->bAllocated || RenderItems.size() >= 255)
        {
            return;
        }

        const uint16 ItemIndex = static_cast<uint16>(RenderItems.size());
        RenderItems.push_back({ Light, Allocation, ShadowView });
        for (FPrimitiveProxy* Proxy : Light->VisibleShadowCasters)
        {
            SCOPE_STAT_CAT("Add ShadowMap Draw Command", "6.Shadow");
            DrawCommandBuild::BuildMeshDrawCommand(*Proxy, ERenderPass::ShadowMap, Context, *Context.DrawCommandList, ItemIndex);
        }
    };

    auto& VisibleLights = Context.Submission.SceneData->Lights.VisibleLightProxies;
    static uint32 LastLoggedVisibleLightCount = UINT32_MAX;
    const uint32 VisibleLightCount = static_cast<uint32>(VisibleLights.size());
    if (LastLoggedVisibleLightCount != VisibleLightCount)
    {
        UE_LOG(Render, Verbose, "Building shadow draw commands for %u visible lights.", VisibleLightCount);
        LastLoggedVisibleLightCount = VisibleLightCount;
    }
    for (FLightProxy* Light : VisibleLights)
    {
        if (!Light || !Light->bCastShadow)
        {
            continue;
        }

        const uint32 LightType = Light->LightProxyInfo.LightType;
        if (LightType == static_cast<uint32>(ELightType::Directional))
        {
            const FCascadeShadowMapData* CascadeShadowMapData = Light->GetCascadeShadowMapData();
            if (!CascadeShadowMapData)
            {
                continue;
            }

            const uint32 MaxAllowedCascades = (GetShadowMapMethod() == EShadowMapMethod::Cascade) ? ShadowAtlas::MaxCascades : 1u;
            const uint32 CascadeCount = std::clamp(CascadeShadowMapData->CascadeCount, 1u, MaxAllowedCascades);
            for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
            {
                AppendRenderItem(
                    Light,
                    &CascadeShadowMapData->Cascades[CascadeIndex],
                    CascadeShadowMapData->CascadeViews[CascadeIndex]);
            }
        }
        else if (LightType == static_cast<uint32>(ELightType::Spot))
        {
            const FShadowMapData* SpotShadowMapData = Light->GetSpotShadowMapData();
            const FShadowViewData* SpotShadowView = Light->GetSpotShadowView();
            if (!SpotShadowMapData || !SpotShadowView)
            {
                continue;
            }

            AppendRenderItem(Light, SpotShadowMapData, *SpotShadowView);
        }
        else if (LightType == static_cast<uint32>(ELightType::Point))
        {
            const FCubeShadowMapData* CubeShadowMapData = Light->GetCubeShadowMapData();
            if (!CubeShadowMapData)
            {
                continue;
            }

            for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
            {
                AppendRenderItem(Light, &CubeShadowMapData->Faces[FaceIndex], CubeShadowMapData->FaceViews[FaceIndex]);
            }
        }
    }
}

void FShadowMapPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FShadowMapPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    static bool bLoggedInvalidItemIndex = false;
    static bool bLoggedMissingAtlasPage = false;
    static bool bLoggedMomentResourceFailure = false;
    static bool bLoggedMissingSliceDSV = false;

    if (!Context.DrawCommandList || RenderItems.empty())
    {
        return;
    }

    uint32 GlobalStart = 0;
    uint32 GlobalEnd = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::ShadowMap, GlobalStart, GlobalEnd);
    if (GlobalStart >= GlobalEnd)
    {
        return;
    }

    ID3D11RenderTargetView* SavedRTV = nullptr;
    ID3D11DepthStencilView* SavedDSV = nullptr;
    Context.Context->OMGetRenderTargets(1, &SavedRTV, &SavedDSV);

    D3D11_VIEWPORT SavedViewport = {};
    uint32 NumViewports = 1;
    Context.Context->RSGetViewports(&NumViewports, &SavedViewport);

    D3D11_RECT SavedScissor = {};
    uint32 NumScissors = 1;
    Context.Context->RSGetScissorRects(&NumScissors, &SavedScissor);

    TArray<FDrawCommand>& Commands = Context.DrawCommandList->GetCommands();
    bool ClearedSlices[ShadowAtlas::MaxPages][ShadowAtlas::SliceCount] = {};

    uint32 CurrentIdx = GlobalStart;
    while (CurrentIdx < GlobalEnd)
    {
        const uint8 ItemIndex = static_cast<uint8>((Commands[CurrentIdx].SortKey >> 52) & 0xFF);
        const uint32 RangeStart = CurrentIdx;
        while (CurrentIdx < GlobalEnd && static_cast<uint8>((Commands[CurrentIdx].SortKey >> 52) & 0xFF) == ItemIndex)
        {
            ++CurrentIdx;
        }
        const uint32 RangeEnd = CurrentIdx;

        if (ItemIndex >= RenderItems.size())
        {
            if (!bLoggedInvalidItemIndex)
            {
                UE_LOG(Render, Warning, "Shadow draw command referenced invalid render item index %u.", ItemIndex);
                bLoggedInvalidItemIndex = true;
            }
            continue;
        }

        const FShadowRenderItem& Item = RenderItems[ItemIndex];
        if (!Item.Allocation || !Item.Allocation->bAllocated)
        {
            continue;
        }

        FShadowAtlasPage* AtlasPage = AtlasPool.GetPage(Item.Allocation->AtlasPageIndex);
        if (!AtlasPage)
        {
            if (!bLoggedMissingAtlasPage)
            {
                UE_LOG(Render, Warning, "Missing shadow atlas page %u for %s.", Item.Allocation->AtlasPageIndex, GetLightLabel(Item.Light).c_str());
                bLoggedMissingAtlasPage = true;
            }
            continue;
        }

        const EShadowFilterMethod ShadowFilterMethod = GetShadowFilterMethod();
        const bool bUsesFilterableMoments =
            ShadowFilterMethod == EShadowFilterMethod::VSM ||
            ShadowFilterMethod == EShadowFilterMethod::ESM;
        if (bUsesFilterableMoments && !AtlasPage->EnsureMomentResources(Context.Device->GetDevice()))
        {
            if (!bLoggedMomentResourceFailure)
            {
                UE_LOG(Render, Warning, "Failed to ensure shadow moment resources for %s at %s.",
                    GetLightLabel(Item.Light).c_str(), FormatShadowAllocation(Item.Allocation).c_str());
                bLoggedMomentResourceFailure = true;
            }
            continue;
        }

        ID3D11DepthStencilView* DSV = AtlasPage->GetSliceDSV(Item.Allocation->SliceIndex);
        ID3D11RenderTargetView* RTV = AtlasPage->GetMomentSliceRTV(Item.Allocation->SliceIndex);
        if (!DSV)
        {
            if (!bLoggedMissingSliceDSV)
            {
                UE_LOG(Render, Warning, "Missing shadow DSV for %s at %s.", GetLightLabel(Item.Light).c_str(), FormatShadowAllocation(Item.Allocation).c_str());
                bLoggedMissingSliceDSV = true;
            }
            continue;
        }

        if (!ClearedSlices[Item.Allocation->AtlasPageIndex][Item.Allocation->SliceIndex])
        {
            Context.Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 0.0f, 0);
            if (RTV)
            {
                float ClearMomentColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                if (ShadowFilterMethod == EShadowFilterMethod::VSM)
                {
                    // Unwritten texels should behave like "no occluder", i.e. the far plane.
                    ClearMomentColor[0] = 1.0f;
                    ClearMomentColor[1] = 1.0f;
                }
                else if (ShadowFilterMethod == EShadowFilterMethod::ESM)
                {
                    const float ShadowESMExponent = Item.Light ? Item.Light->ShadowESMExponent : 40.0f;
                    ClearMomentColor[0] = std::exp(-std::max(ShadowESMExponent, 0.01f));
                }
                Context.Context->ClearRenderTargetView(RTV, ClearMomentColor);
            }
            ClearedSlices[Item.Allocation->AtlasPageIndex][Item.Allocation->SliceIndex] = true;
        }

        D3D11_VIEWPORT ShadowViewport = {};
        ShadowViewport.TopLeftX = static_cast<float>(Item.Allocation->ViewportRect.X);
        ShadowViewport.TopLeftY = static_cast<float>(Item.Allocation->ViewportRect.Y);
        ShadowViewport.Width = static_cast<float>(Item.Allocation->ViewportRect.Width);
        ShadowViewport.Height = static_cast<float>(Item.Allocation->ViewportRect.Height);
        ShadowViewport.MinDepth = 0.0f;
        ShadowViewport.MaxDepth = 1.0f;
        Context.Context->RSSetViewports(1, &ShadowViewport);

        D3D11_RECT ScissorRect = {};
        ScissorRect.left = static_cast<LONG>(Item.Allocation->ViewportRect.X);
        ScissorRect.top = static_cast<LONG>(Item.Allocation->ViewportRect.Y);
        ScissorRect.right = static_cast<LONG>(Item.Allocation->ViewportRect.X + Item.Allocation->ViewportRect.Width);
        ScissorRect.bottom = static_cast<LONG>(Item.Allocation->ViewportRect.Y + Item.Allocation->ViewportRect.Height);
        Context.Context->RSSetScissorRects(1, &ScissorRect);

        Context.Context->OMSetRenderTargets(1, &RTV, DSV);

        FFrameCBData ShadowFrameData = {};
        ShadowFrameData.View = Item.ShadowView.View;
        ShadowFrameData.Projection = Item.ShadowView.Projection;
        ShadowFrameData.InvViewProj = Item.ShadowView.ViewProj.GetInverse();
        Context.Resources->FrameBuffer.Update(Context.Context, &ShadowFrameData, sizeof(FFrameCBData));

        FShadowPassCBData ShadowPassData = {};
        ShadowPassData.ShadowView = Item.ShadowView.View;
        ShadowPassData.ShadowProjection = Item.ShadowView.Projection;
        ShadowPassData.ShadowInvViewProj = Item.ShadowView.ViewProj.GetInverse();
        ShadowPassData.ShadowNearZ = Item.ShadowView.NearZ;
        ShadowPassData.ShadowFarZ = Item.ShadowView.FarZ;
        ShadowPassData.ShadowProjectionType = Item.ShadowView.ProjectionType;
        ShadowPassData.ShadowESMExponent = Item.Light ? Item.Light->ShadowESMExponent : 40.0f;
        Context.Resources->ShadowPassBuffer.Update(Context.Context, &ShadowPassData, sizeof(FShadowPassCBData));

        ID3D11Buffer* ShadowPassCB = Context.Resources->ShadowPassBuffer.GetBuffer();
        Context.Context->VSSetConstantBuffers(ECBSlot::ShadowPass, 1, &ShadowPassCB);
        Context.Context->PSSetConstantBuffers(ECBSlot::ShadowPass, 1, &ShadowPassCB);

        Context.DrawCommandList->SubmitRange(RangeStart, RangeEnd, *Context.Device, Context.Context, *Context.StateCache);
    }

    for (uint32 PageIndex = 0; PageIndex < AtlasPool.GetPageCount(); ++PageIndex)
    {
        FShadowAtlasPage* AtlasPage = AtlasPool.GetPage(PageIndex);
        if (!AtlasPage)
        {
            continue;
        }

        for (uint32 SliceIndex = 0; SliceIndex < ShadowAtlas::SliceCount; ++SliceIndex)
        {
            if (!ClearedSlices[PageIndex][SliceIndex])
            {
                continue;
            }

            MomentFilter.BlurMomentTextureSlice(Context, *AtlasPage, SliceIndex);
        }

        if (ID3D11ShaderResourceView* MomentSRV = AtlasPage->GetMomentArraySRV())
        {
            ID3D11RenderTargetView* NullRTV = nullptr;
            Context.Context->OMSetRenderTargets(1, &NullRTV, nullptr);
            Context.Context->GenerateMips(MomentSRV);
        }
    }

    Context.Context->RSSetViewports(1, &SavedViewport);
    Context.Context->RSSetScissorRects(1, &SavedScissor);
    Context.Context->OMSetRenderTargets(1, &SavedRTV, SavedDSV);
    if (SavedRTV) SavedRTV->Release();
    if (SavedDSV) SavedDSV->Release();

    if (Context.SceneView)
    {
        FFrameCBData MainFrameData = {};
        MainFrameData.View = Context.SceneView->View;
        MainFrameData.Projection = Context.SceneView->Proj;
        MainFrameData.InvViewProj = (Context.SceneView->View * Context.SceneView->Proj).GetInverse();
        MainFrameData.CameraWorldPos = Context.SceneView->CameraPosition;
        MainFrameData.bIsWireframe = (Context.SceneView->ViewMode == EViewMode::Wireframe);
        MainFrameData.WireframeColor = Context.SceneView->WireframeColor;
        Context.Resources->FrameBuffer.Update(Context.Context, &MainFrameData, sizeof(FFrameCBData));
    }
}
