#include "ShadowMapPass.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Renderer.h"
#include "Component/LightComponent.h"

#include <algorithm>
#include <cstring>

FShadowMapPass::~FShadowMapPass()
{
    ReleaseShadowMapResources();
}

void FShadowMapPass::ReleaseShadowMapResources()
{
    for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMaps2DCount; i++)
    {
        if (ShadowResources2D[i].Texture2D) ShadowResources2D[i].Texture2D->Release();
        if (ShadowResources2D[i].DSV2D) ShadowResources2D[i].DSV2D->Release();
        if (ShadowResources2D[i].SRV2D) ShadowResources2D[i].SRV2D->Release();
        if (ShadowResources2D[i].MomentTexture2D) ShadowResources2D[i].MomentTexture2D->Release();
        if (ShadowResources2D[i].MomentRTV2D) ShadowResources2D[i].MomentRTV2D->Release();
        if (ShadowResources2D[i].MomentSRV2D) ShadowResources2D[i].MomentSRV2D->Release();
        if (ShadowResources2D[i].PreviewSRV) ShadowResources2D[i].PreviewSRV->Release();
        ShadowResources2D[i] = {};
    }
    for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMapsCubeCount; i++)
    {
        if (ShadowResourcesCube[i].TextureCube) ShadowResourcesCube[i].TextureCube->Release();
        if (ShadowResourcesCube[i].MomentTextureCube) ShadowResourcesCube[i].MomentTextureCube->Release();
        if (ShadowResourcesCube[i].MomentSRVCube) ShadowResourcesCube[i].MomentSRVCube->Release();
        for (int f = 0; f < 6; ++f)
        {
            if (ShadowResourcesCube[i].DSVCubes[f]) ShadowResourcesCube[i].DSVCubes[f]->Release();
            if (ShadowResourcesCube[i].MomentRTVCubes[f]) ShadowResourcesCube[i].MomentRTVCubes[f]->Release();
            if (ShadowResourcesCube[i].MomentFaceSRVs[f]) ShadowResourcesCube[i].MomentFaceSRVs[f]->Release();
            if (ShadowResourcesCube[i].PreviewSRVs[f]) ShadowResourcesCube[i].PreviewSRVs[f]->Release();
        }
        if (ShadowResourcesCube[i].SRVCube) ShadowResourcesCube[i].SRVCube->Release();
        ShadowResourcesCube[i] = {};
    }

    ReleaseMomentBlurResources();
}

ID3D11ShaderResourceView* FShadowMapPass::GetShadowPreviewSRV(uint32 Index, bool bIsCube, uint32 Face, ID3D11DeviceContext* Context)
{
    (void)Context;

    if (bIsCube)
    {
        if (Index >= ESystemTexSlot::MaxShadowMapsCubeCount || Face >= 6) return nullptr;
        return ShadowResourcesCube[Index].PreviewSRVs[Face];
    }
    else
    {
        if (Index >= ESystemTexSlot::MaxShadowMaps2DCount) return nullptr;
        return ShadowResources2D[Index].PreviewSRV;
    }
}

void FShadowMapPass::SetShadowMapSize(uint32 InShadowMapSize)
{
    const uint32 ClampedSize = std::max(128u, InShadowMapSize);
    if (ShadowMapSize == ClampedSize)
    {
        return;
    }

    ShadowMapSize = ClampedSize;
    ReleaseShadowMapResources();
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

        ID3DBlob* VsBlob           = nullptr;
        ID3DBlob* PsHorizontalBlob = nullptr;
        ID3DBlob* PsVerticalBlob   = nullptr;

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
        CbDesc.ByteWidth         = sizeof(FMomentBlurCBData);
        CbDesc.Usage             = D3D11_USAGE_DYNAMIC;
        CbDesc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
        CbDesc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&CbDesc, nullptr, &MomentBlurCB)))
        {
            return;
        }
    }

    if (MomentBlurTemp2D && MomentBlurTempSize == ShadowMapSize)
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
    TempDesc.Width                = ShadowMapSize;
    TempDesc.Height               = ShadowMapSize;
    TempDesc.MipLevels            = 1;
    TempDesc.ArraySize            = 1;
    TempDesc.Format               = DXGI_FORMAT_R32G32_FLOAT;
    TempDesc.SampleDesc.Count     = 1;
    TempDesc.Usage                = D3D11_USAGE_DEFAULT;
    TempDesc.BindFlags            = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(Device->CreateTexture2D(&TempDesc, nullptr, &MomentBlurTemp2D)))
    {
        ReleaseMomentBlurResources();
        return;
    }
    if (FAILED(Device->CreateRenderTargetView(MomentBlurTemp2D, nullptr, &MomentBlurTempRTV)))
    {
        ReleaseMomentBlurResources();
        return;
    }
    if (FAILED(Device->CreateShaderResourceView(MomentBlurTemp2D, nullptr, &MomentBlurTempSRV)))
    {
        ReleaseMomentBlurResources();
        return;
    }

    MomentBlurTempSize = ShadowMapSize;
}

void FShadowMapPass::ReleaseMomentBlurResources()
{
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
    if (MomentBlurCB)
    {
        MomentBlurCB->Release();
        MomentBlurCB = nullptr;
    }
    if (MomentBlurPSVertical)
    {
        MomentBlurPSVertical->Release();
        MomentBlurPSVertical = nullptr;
    }
    if (MomentBlurPSHorizontal)
    {
        MomentBlurPSHorizontal->Release();
        MomentBlurPSHorizontal = nullptr;
    }
    if (MomentBlurVS)
    {
        MomentBlurVS->Release();
        MomentBlurVS = nullptr;
    }

    MomentBlurTempSize = 0;
}

void FShadowMapPass::BlurMomentTexture2D(FRenderPipelineContext& Context, FShadowResource2D& Resource)
{
    if (GetShadowFilterMethod() != EShadowFilterMethod::VSM)
    {
        return;
    }

    if (!Resource.MomentRTV2D || !Resource.MomentSRV2D)
    {
        return;
    }
    if (!Context.Device)
    {
        return;
    }

    EnsureMomentBlurResources(Context.Device ? Context.Device->GetDevice() : nullptr);
    if (!MomentBlurVS || !MomentBlurPSHorizontal || !MomentBlurPSVertical || !MomentBlurCB || !MomentBlurTempRTV || !MomentBlurTempSRV)
    {
        return;
    }

    ID3D11DeviceContext* Ctx = Context.Context;
    if (!Ctx)
    {
        return;
    }

    FMomentBlurCBData BlurCBData = {};
    BlurCBData.TexelSizeX        = 1.0f / static_cast<float>(std::max(1u, ShadowMapSize));
    BlurCBData.TexelSizeY        = 1.0f / static_cast<float>(std::max(1u, ShadowMapSize));

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(Ctx->Map(MomentBlurCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, &BlurCBData, sizeof(BlurCBData));
        Ctx->Unmap(MomentBlurCB, 0);
    }

    Context.Device->SetDepthStencilState(EDepthStencilState::NoDepth);
    Context.Device->SetBlendState(EBlendState::Opaque);
    Context.Device->SetRasterizerState(ERasterizerState::SolidNoCull);

    Ctx->IASetInputLayout(nullptr);
    Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Ctx->VSSetShader(MomentBlurVS, nullptr, 0);
    Ctx->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &MomentBlurCB);

	Ctx->OMSetRenderTargets(1, &MomentBlurTempRTV, nullptr);

	ID3D11ShaderResourceView* SourceSRV = Resource.MomentSRV2D;
    Ctx->PSSetShader(MomentBlurPSHorizontal, nullptr, 0);
    Ctx->PSSetShaderResources(0, 1, &SourceSRV);
    Ctx->Draw(3, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Ctx->PSSetShaderResources(0, 1, &NullSRV);

	Ctx->OMSetRenderTargets(1, &Resource.MomentRTV2D, nullptr);

	SourceSRV = MomentBlurTempSRV;
    Ctx->PSSetShader(MomentBlurPSVertical, nullptr, 0);
    Ctx->PSSetShaderResources(0, 1, &SourceSRV);
    Ctx->Draw(3, 0);

    Ctx->PSSetShaderResources(0, 1, &NullSRV);
    if (Context.StateCache)
    {
        Context.StateCache->Reset();
    }
}

void FShadowMapPass::BlurMomentTextureCube(FRenderPipelineContext& Context, FShadowResourceCube& Resource)
{
    if (GetShadowFilterMethod() != EShadowFilterMethod::VSM)
    {
        return;
    }

    if (!Context.Device)
    {
        return;
    }

    EnsureMomentBlurResources(Context.Device ? Context.Device->GetDevice() : nullptr);
    if (!MomentBlurVS || !MomentBlurPSHorizontal || !MomentBlurPSVertical || !MomentBlurCB || !MomentBlurTempRTV || !MomentBlurTempSRV)
    {
        return;
    }

    ID3D11DeviceContext* Ctx = Context.Context;
    if (!Ctx)
    {
        return;
    }

    FMomentBlurCBData BlurCBData = {};
    BlurCBData.TexelSizeX        = 1.0f / static_cast<float>(std::max(1u, ShadowMapSize));
    BlurCBData.TexelSizeY        = 1.0f / static_cast<float>(std::max(1u, ShadowMapSize));

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(Ctx->Map(MomentBlurCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, &BlurCBData, sizeof(BlurCBData));
        Ctx->Unmap(MomentBlurCB, 0);
    }

    Context.Device->SetDepthStencilState(EDepthStencilState::NoDepth);
    Context.Device->SetBlendState(EBlendState::Opaque);
    Context.Device->SetRasterizerState(ERasterizerState::SolidNoCull);

    Ctx->IASetInputLayout(nullptr);
    Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Ctx->VSSetShader(MomentBlurVS, nullptr, 0);
    Ctx->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &MomentBlurCB);

    for (int Face = 0; Face < 6; ++Face)
    {
        if (!Resource.MomentRTVCubes[Face] || !Resource.MomentFaceSRVs[Face])
        {
            continue;
        }

        Ctx->OMSetRenderTargets(1, &MomentBlurTempRTV, nullptr);

        ID3D11ShaderResourceView* SourceSRV = Resource.MomentFaceSRVs[Face];
        Ctx->PSSetShader(MomentBlurPSHorizontal, nullptr, 0);
        Ctx->PSSetShaderResources(0, 1, &SourceSRV);
        Ctx->Draw(3, 0);

        ID3D11ShaderResourceView* NullSRV = nullptr;
        Ctx->PSSetShaderResources(0, 1, &NullSRV);

		Ctx->OMSetRenderTargets(1, &Resource.MomentRTVCubes[Face], nullptr);

        SourceSRV = MomentBlurTempSRV;
        Ctx->PSSetShader(MomentBlurPSVertical, nullptr, 0);
        Ctx->PSSetShaderResources(0, 1, &SourceSRV);
        Ctx->Draw(3, 0);

        Ctx->PSSetShaderResources(0, 1, &NullSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->Reset();
    }
}

void FShadowMapPass::PrepareInputs(FRenderPipelineContext& Context)
{
    EnsureShadowMapResources(Context.Device->GetDevice());

    // Clear per-material SRVs (t0 ~ t5)
    ID3D11ShaderResourceView* NullSRVs[6] = {};
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(NullSRVs), NullSRVs);
}

void FShadowMapPass::PrepareTargets(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FShadowMapPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    auto& VisibleLights = Context.Submission.SceneData->Lights.VisibleLightProxies;
    
    for (uint32 i = 0; i < (uint32)VisibleLights.size(); ++i)
    {
        FLightProxy* Light = VisibleLights[i];
        if (!Light || !Light->bCastShadow || Light->ShadowMapIndex < 0) continue;

        FLightProxyInfo& LC = Light->LightProxyInfo;
        uint16 SortKey = static_cast<uint16>(Light->ShadowMapIndex & 0x7F);
        
        // Use 8th bit to distinguish Cube (Point) light shadow commands (UserBits is 8-bit)
        if (LC.LightType == static_cast<uint32>(ELightType::Point))
        {
            SortKey |= 0x80;
        }

        for (FPrimitiveProxy* Proxy : Light->VisibleShadowCasters)
        {
            DrawCommandBuild::BuildMeshDrawCommand(*Proxy, ERenderPass::ShadowMap, Context, *Context.DrawCommandList, SortKey);
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
    if (!Context.DrawCommandList) return;

    uint32 GlobalStart = 0;
    uint32 GlobalEnd = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::ShadowMap, GlobalStart, GlobalEnd);
    if (GlobalStart >= GlobalEnd) return;

    // Save current state
    ID3D11RenderTargetView* SavedRTV = nullptr;
    ID3D11DepthStencilView* SavedDSV = nullptr;
    Context.Context->OMGetRenderTargets(1, &SavedRTV, &SavedDSV);

    D3D11_VIEWPORT SavedViewport;
    uint32 NumViewports = 1;
    Context.Context->RSGetViewports(&NumViewports, &SavedViewport);

    // Set Shadow Viewport
    D3D11_VIEWPORT ShadowViewport = {};
    ShadowViewport.Width = static_cast<float>(ShadowMapSize);
    ShadowViewport.Height = static_cast<float>(ShadowMapSize);
    ShadowViewport.MinDepth = 0.0f;
    ShadowViewport.MaxDepth = 1.0f;
    Context.Context->RSSetViewports(1, &ShadowViewport);

    auto& VisibleLights = Context.Submission.SceneData->Lights.VisibleLightProxies;
    TArray<FDrawCommand>& Commands = Context.DrawCommandList->GetCommands();

    uint32 CurrentIdx = GlobalStart;
    bool ClearedShadow2D[ESystemTexSlot::MaxShadowMaps2DCount] = {};
    bool ClearedShadowCube[ESystemTexSlot::MaxShadowMapsCubeCount] = {};
    // Reversed-Z uses 0.0 as far/max-distance depth, so empty moment texels match the depth clear.
    const float ClearMomentColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    auto ClearShadowForLight = [&](FLightProxy* Light)
    {
        if (!Light || !Light->bCastShadow || Light->ShadowMapIndex < 0)
        {
            return;
        }

        const bool bLightIsCube = (Light->LightProxyInfo.LightType == static_cast<uint32>(ELightType::Point));
        const uint32 ShadowIdx = static_cast<uint32>(Light->ShadowMapIndex);

        if (bLightIsCube)
        {
            if (ShadowIdx >= ESystemTexSlot::MaxShadowMapsCubeCount || ClearedShadowCube[ShadowIdx])
            {
                return;
            }

            FShadowResourceCube& Res = ShadowResourcesCube[ShadowIdx];
            if (!Res.TextureCube)
            {
                return;
            }

            for (int Face = 0; Face < 6; ++Face)
            {
                Context.Context->ClearDepthStencilView(Res.DSVCubes[Face], D3D11_CLEAR_DEPTH, 0.0f, 0);
                if (Res.MomentRTVCubes[Face])
                {
                    Context.Context->ClearRenderTargetView(Res.MomentRTVCubes[Face], ClearMomentColor);
                }
            }
            ClearedShadowCube[ShadowIdx] = true;
            return;
        }

        if (ShadowIdx >= ESystemTexSlot::MaxShadowMaps2DCount || ClearedShadow2D[ShadowIdx])
        {
            return;
        }

        FShadowResource2D& Res = ShadowResources2D[ShadowIdx];
        if (!Res.Texture2D)
        {
            return;
        }

        Context.Context->ClearDepthStencilView(Res.DSV2D, D3D11_CLEAR_DEPTH, 0.0f, 0);
        if (Res.MomentRTV2D)
        {
            Context.Context->ClearRenderTargetView(Res.MomentRTV2D, ClearMomentColor);
        }
        ClearedShadow2D[ShadowIdx] = true;
    };

    // We iterate through commands which are sorted by SortKey (Pass | UserBits | Shader | Mesh | Material)
    while (CurrentIdx < GlobalEnd)
    {
        uint8 UserBits = static_cast<uint8>((Commands[CurrentIdx].SortKey >> 52) & 0xFF);
        uint32 LightStart = CurrentIdx;
        while (CurrentIdx < GlobalEnd && (static_cast<uint8>((Commands[CurrentIdx].SortKey >> 52) & 0xFF)) == UserBits)
        {
            CurrentIdx++;
        }
        uint32 LightEnd = CurrentIdx;

        // Find the light proxy that matches this UserBits
        FLightProxy* TargetLight = nullptr;
        bool bIsCube = (UserBits & 0x80) != 0;
        int32 ShadowIdx = UserBits & 0x7F;

        for (FLightProxy* Light : VisibleLights)
        {
            if (!Light || !Light->bCastShadow) continue;
            
            bool bLightIsCube = (Light->LightProxyInfo.LightType == static_cast<uint32>(ELightType::Point));
            if (bLightIsCube == bIsCube && Light->ShadowMapIndex == ShadowIdx)
            {
                TargetLight = Light;
                break;
            }
        }

        if (TargetLight)
        {
            ClearShadowForLight(TargetLight);

            if (bIsCube)
            {
                FShadowResourceCube& Res = ShadowResourcesCube[ShadowIdx];
                if (Res.TextureCube)
                {
                    for (int Face = 0; Face < 6; ++Face)
                    {
                        ID3D11RenderTargetView* RTVs[] = { Res.MomentRTVCubes[Face] };
                        Context.Context->OMSetRenderTargets(1, RTVs, Res.DSVCubes[Face]);

                        FFrameCBData ShadowFrameData = {};
                        ShadowFrameData.View = FMatrix::Identity;
                        ShadowFrameData.Projection = TargetLight->ShadowViewProjMatrices[Face];
                        ShadowFrameData.InvViewProj = TargetLight->ShadowViewProjMatrices[Face].GetInverse();
                        Context.Resources->FrameBuffer.Update(Context.Context, &ShadowFrameData, sizeof(FFrameCBData));

                        Context.DrawCommandList->SubmitRange(LightStart, LightEnd, *Context.Device, Context.Context, *Context.StateCache);
                    }

					if (Res.MomentSRVCube)
                    {
                        BlurMomentTextureCube(Context, Res);

                        ID3D11RenderTargetView* NullRTVs[1] = { nullptr };
                        Context.Context->OMSetRenderTargets(1, NullRTVs, nullptr);
                        Context.Context->GenerateMips(Res.MomentSRVCube);
                    }
                }
            }
            else
            {
                FShadowResource2D& Res = ShadowResources2D[ShadowIdx];
                if (Res.Texture2D)
                {
                    ID3D11RenderTargetView* RTVs[] = { Res.MomentRTV2D };
                    Context.Context->OMSetRenderTargets(1, RTVs, Res.DSV2D);

                    FFrameCBData ShadowFrameData = {};
                    ShadowFrameData.View = FMatrix::Identity;
                    ShadowFrameData.Projection = TargetLight->LightViewProj;
                    ShadowFrameData.InvViewProj = TargetLight->LightViewProj.GetInverse();
                    Context.Resources->FrameBuffer.Update(Context.Context, &ShadowFrameData, sizeof(FFrameCBData));

                    Context.DrawCommandList->SubmitRange(LightStart, LightEnd, *Context.Device, Context.Context, *Context.StateCache);

					if (Res.MomentSRV2D)
                    {
                        BlurMomentTexture2D(Context, Res);

                        ID3D11RenderTargetView* NullRTVs[1] = { nullptr };
                        Context.Context->OMSetRenderTargets(1, NullRTVs, nullptr);
                        Context.Context->GenerateMips(Res.MomentSRV2D);
                    }
                }
            }
        }
    }

    // If a shadow-casting light has no draw command in this frame, clear its map so stale
    // depth from previous frames does not leak into current lighting.
    for (FLightProxy* Light : VisibleLights)
    {
        ClearShadowForLight(Light);
    }

    // Restore state
    Context.Context->RSSetViewports(1, &SavedViewport);
    Context.Context->OMSetRenderTargets(1, &SavedRTV, SavedDSV);
    if (SavedRTV) SavedRTV->Release();
    if (SavedDSV) SavedDSV->Release();

    // Restore Frame Constants
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

void FShadowMapPass::EnsureShadowMapResources(ID3D11Device* Device)
{
    if (ShadowResources2D[0].Texture2D) return;

    // 1. Texture2D for Directional/Spot
    for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMaps2DCount; ++i)
    {
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = ShadowMapSize;
        texDesc.Height = ShadowMapSize;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        Device->CreateTexture2D(&texDesc, nullptr, &ShadowResources2D[i].Texture2D);

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
        Device->CreateDepthStencilView(ShadowResources2D[i].Texture2D, &dsvDesc, &ShadowResources2D[i].DSV2D);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        Device->CreateShaderResourceView(ShadowResources2D[i].Texture2D, &srvDesc, &ShadowResources2D[i].SRV2D);

        D3D11_TEXTURE2D_DESC momentDesc = {};
        momentDesc.Width = ShadowMapSize;
        momentDesc.Height = ShadowMapSize;
        momentDesc.MipLevels = 0;
        momentDesc.ArraySize = 1;
        momentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        momentDesc.SampleDesc.Count = 1;
        momentDesc.Usage = D3D11_USAGE_DEFAULT;
        momentDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        momentDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

        Device->CreateTexture2D(&momentDesc, nullptr, &ShadowResources2D[i].MomentTexture2D);
        Device->CreateRenderTargetView(ShadowResources2D[i].MomentTexture2D, nullptr, &ShadowResources2D[i].MomentRTV2D);

        D3D11_SHADER_RESOURCE_VIEW_DESC momentSrvDesc = {};
        momentSrvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        momentSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        momentSrvDesc.Texture2D.MipLevels = static_cast<UINT>(-1);
        momentSrvDesc.Texture2D.MostDetailedMip = 0;
        Device->CreateShaderResourceView(ShadowResources2D[i].MomentTexture2D, &momentSrvDesc, &ShadowResources2D[i].MomentSRV2D);

        // Preview SRV (identical to SRV2D for now)
        Device->CreateShaderResourceView(ShadowResources2D[i].Texture2D, &srvDesc, &ShadowResources2D[i].PreviewSRV);
    }

    // 2. TextureCube for Point
    for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMapsCubeCount; ++i)
    {
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = ShadowMapSize;
        texDesc.Height = ShadowMapSize;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 6;
        texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        Device->CreateTexture2D(&texDesc, nullptr, &ShadowResourcesCube[i].TextureCube);

        for (int f = 0; f < 6; ++f)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.FirstArraySlice = f;
            dsvDesc.Texture2DArray.ArraySize = 1;
            dsvDesc.Texture2DArray.MipSlice = 0;
            Device->CreateDepthStencilView(ShadowResourcesCube[i].TextureCube, &dsvDesc, &ShadowResourcesCube[i].DSVCubes[f]);
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;
        Device->CreateShaderResourceView(ShadowResourcesCube[i].TextureCube, &srvDesc, &ShadowResourcesCube[i].SRVCube);

        D3D11_TEXTURE2D_DESC momentDesc = {};
        momentDesc.Width = ShadowMapSize;
        momentDesc.Height = ShadowMapSize;
        momentDesc.MipLevels = 0;
        momentDesc.ArraySize = 6;
        momentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        momentDesc.SampleDesc.Count = 1;
        momentDesc.Usage = D3D11_USAGE_DEFAULT;
        momentDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        momentDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;
        Device->CreateTexture2D(&momentDesc, nullptr, &ShadowResourcesCube[i].MomentTextureCube);

        for (int f = 0; f < 6; ++f)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = f;
            rtvDesc.Texture2DArray.ArraySize = 1;
            Device->CreateRenderTargetView(ShadowResourcesCube[i].MomentTextureCube, &rtvDesc, &ShadowResourcesCube[i].MomentRTVCubes[f]);
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC momentSrvDesc = {};
        momentSrvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        momentSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        momentSrvDesc.TextureCube.MipLevels = static_cast<UINT>(-1);
        momentSrvDesc.TextureCube.MostDetailedMip = 0;
        Device->CreateShaderResourceView(ShadowResourcesCube[i].MomentTextureCube, &momentSrvDesc, &ShadowResourcesCube[i].MomentSRVCube);

        for (int f = 0; f < 6; ++f)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC momentFaceSrvDesc = {};
            momentFaceSrvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
            momentFaceSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            momentFaceSrvDesc.Texture2DArray.MostDetailedMip = 0;
            momentFaceSrvDesc.Texture2DArray.MipLevels = 1;
            momentFaceSrvDesc.Texture2DArray.FirstArraySlice = f;
            momentFaceSrvDesc.Texture2DArray.ArraySize = 1;
            Device->CreateShaderResourceView(ShadowResourcesCube[i].MomentTextureCube, &momentFaceSrvDesc, &ShadowResourcesCube[i].MomentFaceSRVs[f]);
        }

        for (int f = 0; f < 6; ++f)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC previewSrvDesc = {};
            previewSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
            previewSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            previewSrvDesc.Texture2DArray.MostDetailedMip = 0;
            previewSrvDesc.Texture2DArray.MipLevels = 1;
            previewSrvDesc.Texture2DArray.FirstArraySlice = f;
            previewSrvDesc.Texture2DArray.ArraySize = 1;
            Device->CreateShaderResourceView(ShadowResourcesCube[i].TextureCube, &previewSrvDesc, &ShadowResourcesCube[i].PreviewSRVs[f]);
        }
    }
}
