#include "ShadowMapPass.h"

#include "Component/LightComponent.h"
#include "Editor/UI/EditorConsolePanel.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shadows/ShadowMapSettings.h"
#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"

#include <algorithm>
#include <cstring>

namespace
{
struct FShadowDebugPreviewCBData
{
    FMatrix InvViewProj = FMatrix::Identity;
};
} // namespace

FShadowMapPass::~FShadowMapPass()
{
    ReleaseShadowAtlasResources();
}

bool FShadowMapPass::UpdateLightShadowAllocation(FLightProxy& Light, ID3D11Device* Device)
{
    return ShadowRegistry.UpdateLightShadow(Light, Device, AtlasManager);
}

void FShadowMapPass::ReleaseShadowAtlasResources()
{
    ShadowRegistry.Release(AtlasManager);
    AtlasManager.Release();
    ReleaseMomentBlurResources();
    ReleaseDebugPreviewResources();
    RenderItems.clear();
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowAtlasSRV(uint32 PageIndex) const
{
    const FShadowAtlas* Page = AtlasManager.GetPage(PageIndex);
    return Page ? Page->GetDepthArraySRV() : nullptr;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowMomentSRV(uint32 PageIndex) const
{
    const FShadowAtlas* Page = AtlasManager.GetPage(PageIndex);
    return Page ? Page->GetMomentArraySRV() : nullptr;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowPreviewSRV(const FShadowMapData& ShadowMapData) const
{
    if (!ShadowMapData.bAllocated)
    {
        return nullptr;
    }

    const FShadowAtlas* Page = AtlasManager.GetPage(ShadowMapData.AtlasPageIndex);
    return Page ? Page->GetPreviewSliceSRV(ShadowMapData.SliceIndex) : nullptr;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowPageSlicePreviewSRV(uint32 PageIndex, uint32 SliceIndex) const
{
    const FShadowAtlas* Page = AtlasManager.GetPage(PageIndex);
    return Page ? Page->GetPreviewSliceSRV(SliceIndex) : nullptr;
}

void FShadowMapPass::GetShadowPageSliceAllocations(uint32 PageIndex, uint32 SliceIndex, TArray<FShadowMapData>& OutAllocations) const
{
    OutAllocations.clear();

    const FShadowAtlas* Page = AtlasManager.GetPage(PageIndex);
    if (!Page)
    {
        return;
    }

    Page->GatherSliceAllocations(SliceIndex, PageIndex, OutAllocations);
}

uint32 FShadowMapPass::GetShadowAtlasPageCount() const
{
    return AtlasManager.GetPageCount();
}

void FShadowMapPass::EnsureMomentBlurResources(ID3D11Device* Device)
{
    if (Device == nullptr)
    {
        return;
    }

    if (MomentBlurVS == nullptr || MomentBlurPSHorizontal == nullptr || MomentBlurPSVertical == nullptr)
    {
        FShaderStageDesc BlurVSDesc = {};
        BlurVSDesc.FilePath         = "Shaders/Passes/Scene/Shared/ShadowMomentBlurPass.hlsl";
        BlurVSDesc.EntryPoint       = "VS";

        FShaderStageDesc BlurPSHorizontalDesc = {};
        BlurPSHorizontalDesc.FilePath         = "Shaders/Passes/Scene/Shared/ShadowMomentBlurPass.hlsl";
        BlurPSHorizontalDesc.EntryPoint       = "PS_Horizontal";

        FShaderStageDesc BlurPSVerticalDesc = {};
        BlurPSVerticalDesc.FilePath         = "Shaders/Passes/Scene/Shared/ShadowMomentBlurPass.hlsl";
        BlurPSVerticalDesc.EntryPoint       = "PS_Vertical";

        ID3DBlob* VsBlob = nullptr;
        ID3DBlob* PsHorizontalBlob = nullptr;
        ID3DBlob* PsVerticalBlob = nullptr;

        const bool bCompiledVS = FShaderProgramBase::CompileShaderBlobStandalone(
            &VsBlob, BlurVSDesc, "vs_5_0", "Shadow Moment Blur VS Compile Error");
        const bool bCompiledH = FShaderProgramBase::CompileShaderBlobStandalone(
            &PsHorizontalBlob, BlurPSHorizontalDesc, "ps_5_0", "Shadow Moment Blur Horizontal PS Compile Error");
        const bool bCompiledV = FShaderProgramBase::CompileShaderBlobStandalone(
            &PsVerticalBlob, BlurPSVerticalDesc, "ps_5_0", "Shadow Moment Blur Vertical PS Compile Error");
        if (!bCompiledVS || !bCompiledH || !bCompiledV)
        {
            if (VsBlob) VsBlob->Release();
            if (PsHorizontalBlob) PsHorizontalBlob->Release();
            if (PsVerticalBlob) PsVerticalBlob->Release();
            return;
        }

        const HRESULT HrVS = Device->CreateVertexShader(VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), nullptr, &MomentBlurVS);
        const HRESULT HrH = Device->CreatePixelShader(PsHorizontalBlob->GetBufferPointer(), PsHorizontalBlob->GetBufferSize(), nullptr, &MomentBlurPSHorizontal);
        const HRESULT HrV = Device->CreatePixelShader(PsVerticalBlob->GetBufferPointer(), PsVerticalBlob->GetBufferSize(), nullptr, &MomentBlurPSVertical);

        VsBlob->Release();
        PsHorizontalBlob->Release();
        PsVerticalBlob->Release();

        if (FAILED(HrVS) || FAILED(HrH) || FAILED(HrV))
        {
            ReleaseMomentBlurResources();
            return;
        }
    }

    if (MomentBlurCB == nullptr)
    {
        D3D11_BUFFER_DESC CbDesc = {};
        CbDesc.ByteWidth = sizeof(FMomentBlurCBData);
        CbDesc.Usage = D3D11_USAGE_DYNAMIC;
        CbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        CbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&CbDesc, nullptr, &MomentBlurCB)))
        {
            return;
        }
    }

    if (MomentBlurTemp2D && MomentBlurTempSize == ShadowAtlas::AtlasSize)
    {
        return;
    }

    if (MomentBlurTempSRV)
    {
        MomentBlurTempSRV->Release();
        MomentBlurTempSRV = nullptr;
    }
    if (MomentBlurTempRTV)
    {
        MomentBlurTempRTV->Release();
        MomentBlurTempRTV = nullptr;
    }
    if (MomentBlurTemp2D)
    {
        MomentBlurTemp2D->Release();
        MomentBlurTemp2D = nullptr;
    }
    MomentBlurTempSize = 0;

    D3D11_TEXTURE2D_DESC TempDesc = {};
    TempDesc.Width = ShadowAtlas::AtlasSize;
    TempDesc.Height = ShadowAtlas::AtlasSize;
    TempDesc.MipLevels = 1;
    TempDesc.ArraySize = 1;
    TempDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    TempDesc.SampleDesc.Count = 1;
    TempDesc.Usage = D3D11_USAGE_DEFAULT;
    TempDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(Device->CreateTexture2D(&TempDesc, nullptr, &MomentBlurTemp2D)) ||
        FAILED(Device->CreateRenderTargetView(MomentBlurTemp2D, nullptr, &MomentBlurTempRTV)) ||
        FAILED(Device->CreateShaderResourceView(MomentBlurTemp2D, nullptr, &MomentBlurTempSRV)))
    {
        ReleaseMomentBlurResources();
        return;
    }

    MomentBlurTempSize = ShadowAtlas::AtlasSize;
}

void FShadowMapPass::EnsureDebugPreviewResources(ID3D11Device* Device)
{
    if (Device == nullptr)
    {
        return;
    }

    if (DebugPreviewVS == nullptr || DebugPreviewPS == nullptr)
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
            if (VsBlob) VsBlob->Release();
            if (PsBlob) PsBlob->Release();
            return;
        }

        const HRESULT HrVS = Device->CreateVertexShader(VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), nullptr, &DebugPreviewVS);
        const HRESULT HrPS = Device->CreatePixelShader(PsBlob->GetBufferPointer(), PsBlob->GetBufferSize(), nullptr, &DebugPreviewPS);

        VsBlob->Release();
        PsBlob->Release();

        if (FAILED(HrVS) || FAILED(HrPS))
        {
            ReleaseDebugPreviewResources();
            return;
        }
    }

    if (DebugPreviewCB == nullptr)
    {
        D3D11_BUFFER_DESC CbDesc = {};
        CbDesc.ByteWidth = sizeof(FShadowDebugPreviewCBData);
        CbDesc.Usage = D3D11_USAGE_DYNAMIC;
        CbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        CbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&CbDesc, nullptr, &DebugPreviewCB)))
        {
            return;
        }
    }

    if (DebugPreviewTexture && DebugPreviewRTV && DebugPreviewSRV)
    {
        return;
    }

    D3D11_TEXTURE2D_DESC PreviewDesc = {};
    PreviewDesc.Width = DebugPreviewSize;
    PreviewDesc.Height = DebugPreviewSize;
    PreviewDesc.MipLevels = 1;
    PreviewDesc.ArraySize = 1;
    PreviewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    PreviewDesc.SampleDesc.Count = 1;
    PreviewDesc.Usage = D3D11_USAGE_DEFAULT;
    PreviewDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(Device->CreateTexture2D(&PreviewDesc, nullptr, &DebugPreviewTexture)) ||
        FAILED(Device->CreateRenderTargetView(DebugPreviewTexture, nullptr, &DebugPreviewRTV)) ||
        FAILED(Device->CreateShaderResourceView(DebugPreviewTexture, nullptr, &DebugPreviewSRV)))
    {
        ReleaseDebugPreviewResources();
        return;
    }
}

void FShadowMapPass::ReleaseMomentBlurResources()
{
    if (MomentBlurTempSRV) { MomentBlurTempSRV->Release(); MomentBlurTempSRV = nullptr; }
    if (MomentBlurTempRTV) { MomentBlurTempRTV->Release(); MomentBlurTempRTV = nullptr; }
    if (MomentBlurTemp2D) { MomentBlurTemp2D->Release(); MomentBlurTemp2D = nullptr; }
    if (MomentBlurCB) { MomentBlurCB->Release(); MomentBlurCB = nullptr; }
    if (MomentBlurPSVertical) { MomentBlurPSVertical->Release(); MomentBlurPSVertical = nullptr; }
    if (MomentBlurPSHorizontal) { MomentBlurPSHorizontal->Release(); MomentBlurPSHorizontal = nullptr; }
    if (MomentBlurVS) { MomentBlurVS->Release(); MomentBlurVS = nullptr; }
    MomentBlurTempSize = 0;
}

void FShadowMapPass::ReleaseDebugPreviewResources()
{
    if (DebugPreviewSRV) { DebugPreviewSRV->Release(); DebugPreviewSRV = nullptr; }
    if (DebugPreviewRTV) { DebugPreviewRTV->Release(); DebugPreviewRTV = nullptr; }
    if (DebugPreviewTexture) { DebugPreviewTexture->Release(); DebugPreviewTexture = nullptr; }
    if (DebugPreviewCB) { DebugPreviewCB->Release(); DebugPreviewCB = nullptr; }
    if (DebugPreviewPS) { DebugPreviewPS->Release(); DebugPreviewPS = nullptr; }
    if (DebugPreviewVS) { DebugPreviewVS->Release(); DebugPreviewVS = nullptr; }
}

void FShadowMapPass::BlurMomentTextureSlice(FRenderPipelineContext& Context, FShadowAtlas& AtlasPage, uint32 SliceIndex)
{
    if (GetShadowFilterMethod() != EShadowFilterMethod::VSM || !Context.Device || !Context.Context)
    {
        return;
    }

    EnsureMomentBlurResources(Context.Device->GetDevice());
    if (!MomentBlurVS || !MomentBlurPSHorizontal || !MomentBlurPSVertical || !MomentBlurCB || !MomentBlurTempRTV || !MomentBlurTempSRV)
    {
        return;
    }

    ID3D11RenderTargetView* TargetRTV = AtlasPage.GetMomentSliceRTV(SliceIndex);
    ID3D11ShaderResourceView* TargetSRV = AtlasPage.GetMomentSliceSRV(SliceIndex);
    if (!TargetRTV || !TargetSRV)
    {
        return;
    }

    FMomentBlurCBData BlurCBData = {};
    BlurCBData.TexelSizeX = 1.0f / static_cast<float>(ShadowAtlas::AtlasSize);
    BlurCBData.TexelSizeY = 1.0f / static_cast<float>(ShadowAtlas::AtlasSize);

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(Context.Context->Map(MomentBlurCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, &BlurCBData, sizeof(BlurCBData));
        Context.Context->Unmap(MomentBlurCB, 0);
    }

    Context.Device->SetDepthStencilState(EDepthStencilState::NoDepth);
    Context.Device->SetBlendState(EBlendState::Opaque);
    Context.Device->SetRasterizerState(ERasterizerState::SolidNoCull);

    // Moment blur는 마지막 shadow rect viewport를 이어받으면 안 되므로 slice 전체 viewport로 복원합니다.
    D3D11_VIEWPORT FullSliceViewport = {};
    FullSliceViewport.TopLeftX = 0.0f;
    FullSliceViewport.TopLeftY = 0.0f;
    FullSliceViewport.Width = static_cast<float>(ShadowAtlas::AtlasSize);
    FullSliceViewport.Height = static_cast<float>(ShadowAtlas::AtlasSize);
    FullSliceViewport.MinDepth = 0.0f;
    FullSliceViewport.MaxDepth = 1.0f;
    Context.Context->RSSetViewports(1, &FullSliceViewport);

    D3D11_RECT FullSliceScissor = {};
    FullSliceScissor.left = 0;
    FullSliceScissor.top = 0;
    FullSliceScissor.right = static_cast<LONG>(ShadowAtlas::AtlasSize);
    FullSliceScissor.bottom = static_cast<LONG>(ShadowAtlas::AtlasSize);
    Context.Context->RSSetScissorRects(1, &FullSliceScissor);

    Context.Context->IASetInputLayout(nullptr);
    Context.Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context.Context->VSSetShader(MomentBlurVS, nullptr, 0);
    Context.Context->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &MomentBlurCB);

    Context.Context->OMSetRenderTargets(1, &MomentBlurTempRTV, nullptr);
    Context.Context->PSSetShader(MomentBlurPSHorizontal, nullptr, 0);
    Context.Context->PSSetShaderResources(0, 1, &TargetSRV);
    Context.Context->Draw(3, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context.Context->PSSetShaderResources(0, 1, &NullSRV);

    Context.Context->OMSetRenderTargets(1, &TargetRTV, nullptr);
    Context.Context->PSSetShader(MomentBlurPSVertical, nullptr, 0);
    Context.Context->PSSetShaderResources(0, 1, &MomentBlurTempSRV);
    Context.Context->Draw(3, 0);
    Context.Context->PSSetShaderResources(0, 1, &NullSRV);

    if (Context.StateCache)
    {
        Context.StateCache->Reset();
    }
}

bool FShadowMapPass::HasPSMCameraChanged(const FSceneView& SceneView)
{
    const bool bChanged =
        !bHasLastPSMCamera ||
        FVector::DistSquared(LastPSMCameraPosition, SceneView.CameraPosition) > 1e-4f ||
        FVector::DistSquared(LastPSMCameraForward, SceneView.CameraForward) > 1e-6f ||
        FVector::DistSquared(LastPSMCameraUp, SceneView.CameraUp) > 1e-6f;

    LastPSMCameraPosition = SceneView.CameraPosition;
    LastPSMCameraForward = SceneView.CameraForward;
    LastPSMCameraUp = SceneView.CameraUp;
    bHasLastPSMCamera = true;
    return bChanged;
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowDebugPreviewSRV(
    const FShadowMapData& ShadowMapData,
    const FMatrix&        ViewProj,
    ID3D11Device*         Device,
    ID3D11DeviceContext*  DeviceContext)
{
    if (!ShadowMapData.bAllocated || Device == nullptr || DeviceContext == nullptr)
    {
        return nullptr;
    }

    EnsureDebugPreviewResources(Device);
    if (!DebugPreviewVS || !DebugPreviewPS || !DebugPreviewCB || !DebugPreviewRTV || !DebugPreviewSRV)
    {
        return nullptr;
    }

    ID3D11ShaderResourceView* SourceSRV = GetShadowPreviewSRV(ShadowMapData);
    if (!SourceSRV)
    {
        return nullptr;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(DeviceContext->Map(DebugPreviewCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        FShadowDebugPreviewCBData PreviewCBData = {};
        PreviewCBData.InvViewProj = ViewProj.GetInverse();
        std::memcpy(Mapped.pData, &PreviewCBData, sizeof(PreviewCBData));
        DeviceContext->Unmap(DebugPreviewCB, 0);
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
    DeviceContext->ClearRenderTargetView(DebugPreviewRTV, ClearColor);

    D3D11_VIEWPORT PreviewViewport = {};
    PreviewViewport.TopLeftX = 0.0f;
    PreviewViewport.TopLeftY = 0.0f;
    PreviewViewport.Width = static_cast<float>(DebugPreviewSize);
    PreviewViewport.Height = static_cast<float>(DebugPreviewSize);
    PreviewViewport.MinDepth = 0.0f;
    PreviewViewport.MaxDepth = 1.0f;
    DeviceContext->RSSetViewports(1, &PreviewViewport);

    D3D11_RECT PreviewScissor = {};
    PreviewScissor.left = 0;
    PreviewScissor.top = 0;
    PreviewScissor.right = static_cast<LONG>(DebugPreviewSize);
    PreviewScissor.bottom = static_cast<LONG>(DebugPreviewSize);
    DeviceContext->RSSetScissorRects(1, &PreviewScissor);

    DeviceContext->IASetInputLayout(nullptr);
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->VSSetShader(DebugPreviewVS, nullptr, 0);
    DeviceContext->PSSetShader(DebugPreviewPS, nullptr, 0);
    DeviceContext->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &DebugPreviewCB);
    DeviceContext->OMSetRenderTargets(1, &DebugPreviewRTV, nullptr);
    DeviceContext->PSSetShaderResources(0, 1, &SourceSRV);
    DeviceContext->Draw(3, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    DeviceContext->PSSetShaderResources(0, 1, &NullSRV);

    DeviceContext->RSSetViewports(1, &SavedViewport);
    DeviceContext->RSSetScissorRects(1, &SavedScissor);
    DeviceContext->OMSetRenderTargets(1, &SavedRTV, SavedDSV);
    if (SavedRTV) SavedRTV->Release();
    if (SavedDSV) SavedDSV->Release();

    return DebugPreviewSRV;
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
    bLoggedPSMRedrawThisFrame = false;

    const bool bPSM = GetShadowMapMethod() == EShadowMapMethod::PSM;
    if (bPSM && Context.SceneView && HasPSMCameraChanged(*Context.SceneView))
    {
        bLoggedPSMRedrawThisFrame = true;
        UE_LOG(
            "[Shadow][PSM] camera changed -> rebuild shadow map commands. CamPos=(%.2f, %.2f, %.2f) CamFwd=(%.3f, %.3f, %.3f) VisibleLights=%zu",
            Context.SceneView->CameraPosition.X,
            Context.SceneView->CameraPosition.Y,
            Context.SceneView->CameraPosition.Z,
            Context.SceneView->CameraForward.X,
            Context.SceneView->CameraForward.Y,
            Context.SceneView->CameraForward.Z,
            Context.Submission.SceneData ? Context.Submission.SceneData->Lights.VisibleLightProxies.size() : 0ull);
    }

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
            DrawCommandBuild::BuildMeshDrawCommand(*Proxy, ERenderPass::ShadowMap, Context, *Context.DrawCommandList, ItemIndex);
        }
    };

    auto& VisibleLights = Context.Submission.SceneData->Lights.VisibleLightProxies;
    for (FLightProxy* Light : VisibleLights)
    {
        if (!Light || !Light->bCastShadow)
        {
            continue;
        }

        const uint32 LightType = Light->LightProxyInfo.LightType;
        if (LightType == static_cast<uint32>(ELightType::Directional))
        {
            const uint32 CascadeCount = std::max(1u, Light->CascadeShadowMapData.CascadeCount);
            for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
            {
                if (bLoggedPSMRedrawThisFrame && GetShadowMapMethod() == EShadowMapMethod::PSM && CascadeIndex == 0)
                {
                    const uint64* HashBytes = reinterpret_cast<const uint64*>(Light->LightViewProj.Data);
                    UE_LOG(
                        "[Shadow][PSM] directional light hash sample=%016llX-%016llX casters=%zu cascadeCount=%u",
                        HashBytes[0],
                        HashBytes[1],
                        Light->VisibleShadowCasters.size(),
                        CascadeCount);
                }
                AppendRenderItem(
                    Light,
                    &Light->CascadeShadowMapData.Cascades[CascadeIndex],
                    Light->CascadeShadowMapData.CascadeViews[CascadeIndex]);
            }
        }
        else if (LightType == static_cast<uint32>(ELightType::Spot))
        {
            AppendRenderItem(Light, &Light->SpotShadowMapData, Light->LightShadowView);
        }
        else if (LightType == static_cast<uint32>(ELightType::Point))
        {
            for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
            {
                AppendRenderItem(Light, &Light->CubeShadowMapData.Faces[FaceIndex], Light->CubeShadowMapData.FaceViews[FaceIndex]);
            }
        }
    }

    if (bLoggedPSMRedrawThisFrame)
    {
        uint64 TotalShadowCasters = 0;
        auto& VisibleLights = Context.Submission.SceneData->Lights.VisibleLightProxies;
        for (FLightProxy* Light : VisibleLights)
        {
            if (Light && Light->bCastShadow)
            {
                TotalShadowCasters += static_cast<uint64>(Light->VisibleShadowCasters.size());
            }
        }

        UE_LOG(
            "[Shadow][PSM] built render items=%zu totalShadowCasters=%llu",
            RenderItems.size(),
            TotalShadowCasters);
    }
}

void FShadowMapPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FShadowMapPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.DrawCommandList || RenderItems.empty())
    {
        if (bLoggedPSMRedrawThisFrame)
        {
            UE_LOG("[Shadow][PSM] submit skipped. RenderItems=%zu", RenderItems.size());
        }
        return;
    }

    uint32 GlobalStart = 0;
    uint32 GlobalEnd = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::ShadowMap, GlobalStart, GlobalEnd);
    if (GlobalStart >= GlobalEnd)
    {
        if (bLoggedPSMRedrawThisFrame)
        {
            UE_LOG("[Shadow][PSM] submit skipped. ShadowMap pass range empty. Start=%u End=%u", GlobalStart, GlobalEnd);
        }
        return;
    }

    if (bLoggedPSMRedrawThisFrame)
    {
        UE_LOG(
            "[Shadow][PSM] submitting shadow map draw range Start=%u End=%u RenderItems=%zu",
            GlobalStart,
            GlobalEnd,
            RenderItems.size());
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
            continue;
        }

        const FShadowRenderItem& Item = RenderItems[ItemIndex];
        if (!Item.Allocation || !Item.Allocation->bAllocated)
        {
            continue;
        }

        FShadowAtlas* AtlasPage = AtlasManager.GetPage(Item.Allocation->AtlasPageIndex);
        if (!AtlasPage)
        {
            continue;
        }

        ID3D11DepthStencilView* DSV = AtlasPage->GetSliceDSV(Item.Allocation->SliceIndex);
        ID3D11RenderTargetView* RTV = AtlasPage->GetMomentSliceRTV(Item.Allocation->SliceIndex);
        if (!DSV)
        {
            continue;
        }

        if (!ClearedSlices[Item.Allocation->AtlasPageIndex][Item.Allocation->SliceIndex])
        {
            Context.Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 0.0f, 0);
            if (RTV)
            {
                const float ClearMomentColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
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
        Context.Resources->ShadowPassBuffer.Update(Context.Context, &ShadowPassData, sizeof(FShadowPassCBData));

        ID3D11Buffer* ShadowPassCB = Context.Resources->ShadowPassBuffer.GetBuffer();
        Context.Context->VSSetConstantBuffers(ECBSlot::ShadowPass, 1, &ShadowPassCB);
        Context.Context->PSSetConstantBuffers(ECBSlot::ShadowPass, 1, &ShadowPassCB);

        Context.DrawCommandList->SubmitRange(RangeStart, RangeEnd, *Context.Device, Context.Context, *Context.StateCache);
    }

    for (uint32 PageIndex = 0; PageIndex < AtlasManager.GetPageCount(); ++PageIndex)
    {
        FShadowAtlas* AtlasPage = AtlasManager.GetPage(PageIndex);
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

            BlurMomentTextureSlice(Context, *AtlasPage, SliceIndex);
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
