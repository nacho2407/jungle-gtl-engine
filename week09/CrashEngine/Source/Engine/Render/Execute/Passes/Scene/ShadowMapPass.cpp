#include "ShadowMapPass.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Renderer.h"
#include "Component/LightComponent.h"

#include <algorithm>

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
        if (ShadowResources2D[i].PreviewSRV) ShadowResources2D[i].PreviewSRV->Release();
        ShadowResources2D[i] = {};
    }
    for (uint32 i = 0; i < ESystemTexSlot::MaxShadowMapsCubeCount; i++)
    {
        if (ShadowResourcesCube[i].TextureCube) ShadowResourcesCube[i].TextureCube->Release();
        for (int f = 0; f < 6; ++f)
        {
            if (ShadowResourcesCube[i].DSVCubes[f]) ShadowResourcesCube[i].DSVCubes[f]->Release();
            if (ShadowResourcesCube[i].PreviewSRVs[f]) ShadowResourcesCube[i].PreviewSRVs[f]->Release();
        }
        if (ShadowResourcesCube[i].SRVCube) ShadowResourcesCube[i].SRVCube->Release();
        ShadowResourcesCube[i] = {};
    }
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
            if (bIsCube)
            {
                FShadowResourceCube& Res = ShadowResourcesCube[ShadowIdx];
                if (Res.TextureCube)
                {
                    for (int Face = 0; Face < 6; ++Face)
                    {
                        Context.Context->OMSetRenderTargets(0, nullptr, Res.DSVCubes[Face]);
                        Context.Context->ClearDepthStencilView(Res.DSVCubes[Face], D3D11_CLEAR_DEPTH, 0.0f, 0);

                        FFrameCBData ShadowFrameData = {};
                        ShadowFrameData.View = FMatrix::Identity;
                        ShadowFrameData.Projection = TargetLight->ShadowViewProjMatrices[Face];
                        ShadowFrameData.InvViewProj = TargetLight->ShadowViewProjMatrices[Face].GetInverse();
                        Context.Resources->FrameBuffer.Update(Context.Context, &ShadowFrameData, sizeof(FFrameCBData));

                        Context.DrawCommandList->SubmitRange(LightStart, LightEnd, *Context.Device, Context.Context, *Context.StateCache);
                    }
                }
            }
            else
            {
                FShadowResource2D& Res = ShadowResources2D[ShadowIdx];
                if (Res.Texture2D)
                {
                    Context.Context->OMSetRenderTargets(0, nullptr, Res.DSV2D);
                    Context.Context->ClearDepthStencilView(Res.DSV2D, D3D11_CLEAR_DEPTH, 0.0f, 0);

                    FFrameCBData ShadowFrameData = {};
                    ShadowFrameData.View = FMatrix::Identity;
                    ShadowFrameData.Projection = TargetLight->LightViewProj;
                    ShadowFrameData.InvViewProj = TargetLight->LightViewProj.GetInverse();
                    Context.Resources->FrameBuffer.Update(Context.Context, &ShadowFrameData, sizeof(FFrameCBData));

                    Context.DrawCommandList->SubmitRange(LightStart, LightEnd, *Context.Device, Context.Context, *Context.StateCache);
                }
            }
        }
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
