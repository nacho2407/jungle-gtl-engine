#include "ShadowMapPass.h"

#include "Component/LightComponent.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"

#include <algorithm>
#include <cstring>

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

    auto AppendRenderItem = [&](FLightProxy* Light, const FShadowMapData* Allocation, const FMatrix& ViewProj)
    {
        if (!Light || !Allocation || !Allocation->bAllocated || RenderItems.size() >= 255)
        {
            return;
        }

        const uint16 ItemIndex = static_cast<uint16>(RenderItems.size());
        RenderItems.push_back({ Light, Allocation, ViewProj });
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
                AppendRenderItem(
                    Light,
                    &Light->CascadeShadowMapData.Cascades[CascadeIndex],
                    Light->CascadeShadowMapData.CascadeViewProj[CascadeIndex]);
            }
        }
        else if (LightType == static_cast<uint32>(ELightType::Spot))
        {
            AppendRenderItem(Light, &Light->SpotShadowMapData, Light->LightViewProj);
        }
        else if (LightType == static_cast<uint32>(ELightType::Point))
        {
            for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
            {
                AppendRenderItem(Light, &Light->CubeShadowMapData.Faces[FaceIndex], Light->CubeShadowMapData.FaceViewProj[FaceIndex]);
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
        ShadowFrameData.View = FMatrix::Identity;
        ShadowFrameData.Projection = Item.ViewProj;
        ShadowFrameData.InvViewProj = Item.ViewProj.GetInverse();
        Context.Resources->FrameBuffer.Update(Context.Context, &ShadowFrameData, sizeof(FFrameCBData));

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
