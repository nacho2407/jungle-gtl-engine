#include "ShadowMapPass.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Renderer.h"
#include "Component/LightComponent.h"

FShadowMapPass::~FShadowMapPass()
{
    for (uint32 i = 0; i < MAX_SHADOW_MAPS; ++i)
    {
        if (ShadowResources[i].Texture) ShadowResources[i].Texture->Release();
        for (int f = 0; f < 6; ++f)
        {
            if (ShadowResources[i].DSVs[f]) ShadowResources[i].DSVs[f]->Release();
        }
        if (ShadowResources[i].SRV) ShadowResources[i].SRV->Release();
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
    uint32 ShadowLightCount = 0;
    for (uint32 i = 0; i < (uint32)VisibleLights.size(); ++i)
    {
        FLightProxy* Light = VisibleLights[i];
        if (!Light || !Light->bCastShadow) continue;

        // Only generate commands for the first MAX_SHADOW_MAPS lights
        if (ShadowLightCount >= MAX_SHADOW_MAPS) break;

        for (FPrimitiveProxy* Proxy : Light->VisibleShadowCasters)
        {
            // UserBits stores the shadow map index (0~4)
            DrawCommandBuild::BuildMeshDrawCommand(*Proxy, ERenderPass::ShadowMap, Context, *Context.DrawCommandList, static_cast<uint16>(ShadowLightCount));
        }
        ShadowLightCount++;
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
    uint32 ShadowLightIndex = 0;

    for (uint32 i = 0; i < (uint32)VisibleLights.size(); ++i)
    {
        FLightProxy* Light = VisibleLights[i];
        if (!Light || !Light->bCastShadow) continue;
        if (ShadowLightIndex >= MAX_SHADOW_MAPS) break;

        FShadowResource& Res = ShadowResources[ShadowLightIndex];
        if (!Res.Texture) { ShadowLightIndex++; continue; }

        // Find range for this specific shadow map index
        uint32 LightStart = CurrentIdx;
        while (CurrentIdx < GlobalEnd && (Commands[CurrentIdx].SortKey & 0xFFF) == ShadowLightIndex)
        {
            CurrentIdx++;
        }
        uint32 LightEnd = CurrentIdx;

        if (LightStart < LightEnd)
        {
            FLightProxyInfo& LC = Light->LightProxyInfo;

            if (LC.LightType == static_cast<uint32>(ELightType::Point))
            {
                for (int Face = 0; Face < 6; ++Face)
                {
                    Context.Context->OMSetRenderTargets(0, nullptr, Res.DSVs[Face]);
                    Context.Context->ClearDepthStencilView(Res.DSVs[Face], D3D11_CLEAR_DEPTH, 0.0f, 0);

                    FFrameCBData ShadowFrameData = {};
                    ShadowFrameData.View = FMatrix::Identity;
                    ShadowFrameData.Projection = Light->ShadowViewProjMatrices[Face];
                    ShadowFrameData.InvViewProj = Light->ShadowViewProjMatrices[Face].GetInverse();
                    Context.Resources->FrameBuffer.Update(Context.Context, &ShadowFrameData, sizeof(FFrameCBData));

                    Context.DrawCommandList->SubmitRange(LightStart, LightEnd, *Context.Device, Context.Context, *Context.StateCache);
                }
            }
            else
            {
                Context.Context->OMSetRenderTargets(0, nullptr, Res.DSVs[0]);
                Context.Context->ClearDepthStencilView(Res.DSVs[0], D3D11_CLEAR_DEPTH, 0.0f, 0);

                FFrameCBData ShadowFrameData = {};
                ShadowFrameData.View = FMatrix::Identity;
                ShadowFrameData.Projection = Light->LightViewProj;
                ShadowFrameData.InvViewProj = Light->LightViewProj.GetInverse();
                Context.Resources->FrameBuffer.Update(Context.Context, &ShadowFrameData, sizeof(FFrameCBData));

                Context.DrawCommandList->SubmitRange(LightStart, LightEnd, *Context.Device, Context.Context, *Context.StateCache);
            }
        }

        ShadowLightIndex++;
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
    if (ShadowResources[0].Texture) return;

    for (uint32 i = 0; i < MAX_SHADOW_MAPS; ++i)
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

        Device->CreateTexture2D(&texDesc, nullptr, &ShadowResources[i].Texture);

        for (int f = 0; f < 6; ++f)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.FirstArraySlice = f;
            dsvDesc.Texture2DArray.ArraySize = 1;
            dsvDesc.Texture2DArray.MipSlice = 0;
            Device->CreateDepthStencilView(ShadowResources[i].Texture, &dsvDesc, &ShadowResources[i].DSVs[f]);
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;
        Device->CreateShaderResourceView(ShadowResources[i].Texture, &srvDesc, &ShadowResources[i].SRV);
    }
}
