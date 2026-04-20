#include "Render/Pipelines/Context/View/ViewModeSurfaceSet.h"

bool FViewModeSurfaceSet::Initialize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    Release();

    Width = InWidth;
    Height = InHeight;

    for (uint32 i = 0; i < static_cast<uint32>(ESurfaceSlot::Count); ++i)
    {
        if (!CreateSurface(Device, static_cast<ESurfaceSlot>(i), DXGI_FORMAT_R8G8B8A8_UNORM, Width, Height))
        {
            Release();
            return false;
        }
    }

    return true;
}

void FViewModeSurfaceSet::Resize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
    if (InWidth == Width && InHeight == Height)
    {
        return;
    }

    Initialize(Device, InWidth, InHeight);
}

void FViewModeSurfaceSet::Release()
{
    for (uint32 i = 0; i < static_cast<uint32>(ESurfaceSlot::Count); ++i)
    {
        ReleaseSurface(Surfaces[i]);
    }

    Width = 0;
    Height = 0;
}

void FViewModeSurfaceSet::ClearBaseTargets(ID3D11DeviceContext* Ctx, EShadingModel Model)
{
    const float ClearColor[4] = { 0, 0, 0, 0 };
    Ctx->ClearRenderTargetView(GetRTV(ESurfaceSlot::BaseColor), ClearColor);

    if (Model == EShadingModel::Gouraud || Model == EShadingModel::Lambert || Model == EShadingModel::BlinnPhong)
    {
        Ctx->ClearRenderTargetView(GetRTV(ESurfaceSlot::Surface1), ClearColor);
    }

    if (Model == EShadingModel::BlinnPhong)
    {
        Ctx->ClearRenderTargetView(GetRTV(ESurfaceSlot::Surface2), ClearColor);
    }
}

void FViewModeSurfaceSet::ClearModifiedTargets(ID3D11DeviceContext* Ctx, EShadingModel Model)
{
    const float ClearColor[4] = { 0, 0, 0, 0 };
    Ctx->ClearRenderTargetView(GetRTV(ESurfaceSlot::ModifiedBaseColor), ClearColor);

    if (Model == EShadingModel::Lambert || Model == EShadingModel::BlinnPhong)
    {
        Ctx->ClearRenderTargetView(GetRTV(ESurfaceSlot::ModifiedSurface1), ClearColor);
    }

    if (Model == EShadingModel::BlinnPhong)
    {
        Ctx->ClearRenderTargetView(GetRTV(ESurfaceSlot::ModifiedSurface2), ClearColor);
    }
}

void FViewModeSurfaceSet::BindBaseDrawTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV)
{
    ID3D11RenderTargetView* Targets[3] = {
        GetRTV(ESurfaceSlot::BaseColor),
        nullptr,
        nullptr,
    };
    uint32 TargetCount = 1;

    if (Model == EShadingModel::Gouraud || Model == EShadingModel::Lambert || Model == EShadingModel::BlinnPhong)
    {
        Targets[1] = GetRTV(ESurfaceSlot::Surface1);
        TargetCount = 2;
    }

    if (Model == EShadingModel::BlinnPhong)
    {
        Targets[2] = GetRTV(ESurfaceSlot::Surface2);
        TargetCount = 3;
    }

    Ctx->OMSetRenderTargets(TargetCount, Targets, DSV);
}

void FViewModeSurfaceSet::BindDecalTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV)
{
    ID3D11RenderTargetView* Targets[3] = {
        GetRTV(ESurfaceSlot::ModifiedBaseColor),
        nullptr,
        nullptr,
    };
    uint32 TargetCount = 1;

    if (Model == EShadingModel::Lambert || Model == EShadingModel::BlinnPhong)
    {
        Targets[1] = GetRTV(ESurfaceSlot::ModifiedSurface1);
        TargetCount = 2;
    }

    if (Model == EShadingModel::BlinnPhong)
    {
        Targets[2] = GetRTV(ESurfaceSlot::ModifiedSurface2);
        TargetCount = 3;
    }

    Ctx->OMSetRenderTargets(TargetCount, Targets, DSV);
}

ID3D11ShaderResourceView* FViewModeSurfaceSet::GetSRV(ESurfaceSlot Slot) const
{
    return Surfaces[static_cast<uint32>(Slot)].SRV;
}

ID3D11RenderTargetView* FViewModeSurfaceSet::GetRTV(ESurfaceSlot Slot) const
{
    return Surfaces[static_cast<uint32>(Slot)].RTV;
}

bool FViewModeSurfaceSet::CreateSurface(ID3D11Device* Device, ESurfaceSlot Slot, DXGI_FORMAT Format, uint32 InWidth, uint32 InHeight)
{
    FSurfaceTexture& Surface = Surfaces[static_cast<uint32>(Slot)];
    Surface.Format = Format;

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = InWidth;
    Desc.Height = InHeight;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = Format;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(Device->CreateTexture2D(&Desc, nullptr, &Surface.Texture)))
    {
        return false;
    }
    if (FAILED(Device->CreateRenderTargetView(Surface.Texture, nullptr, &Surface.RTV)))
    {
        return false;
    }
    if (FAILED(Device->CreateShaderResourceView(Surface.Texture, nullptr, &Surface.SRV)))
    {
        return false;
    }

    return true;
}

void FViewModeSurfaceSet::ReleaseSurface(FSurfaceTexture& Surface)
{
    if (Surface.SRV)
    {
        Surface.SRV->Release();
        Surface.SRV = nullptr;
    }
    if (Surface.RTV)
    {
        Surface.RTV->Release();
        Surface.RTV = nullptr;
    }
    if (Surface.Texture)
    {
        Surface.Texture->Release();
        Surface.Texture = nullptr;
    }

    Surface.Format = DXGI_FORMAT_UNKNOWN;
}
