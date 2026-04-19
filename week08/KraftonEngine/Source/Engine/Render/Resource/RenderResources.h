#pragma once
#include "Render/Resource/Buffer.h"
#include "Render/Core/RenderConstants.h"

#include "Render/Types/RenderTypes.h"
#include "Render/Types/ShadingTypes.h"

struct FSurfaceTexture
{
    ID3D11Texture2D *Texture = nullptr;
    ID3D11RenderTargetView *RTV = nullptr;
    ID3D11ShaderResourceView *SRV = nullptr;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

enum class ESurfaceSlot : uint8
{
    BaseColor = 0,
    Surface1,
    Surface2,
    ModifiedBaseColor,
    ModifiedSurface1,
    ModifiedSurface2,
    Count
};

class FViewModeSurfaceSet
{
  public:
    bool Initialize(ID3D11Device *Device, uint32 InWidth, uint32 InHeight)
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

    void Resize(ID3D11Device *Device, uint32 InWidth, uint32 InHeight)
    {
        if (InWidth == Width && InHeight == Height)
        {
            return;
        }

        Initialize(Device, InWidth, InHeight);
    }

    void Release()
    {
        for (uint32 i = 0; i < static_cast<uint32>(ESurfaceSlot::Count); ++i)
        {
            ReleaseSurface(Surfaces[i]);
        }

        Width = 0;
        Height = 0;
    }

    void ClearBaseTargets(ID3D11DeviceContext *Ctx, EShadingModel Model)
    {
        const float ClearColor[4] = {0, 0, 0, 0};
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

    void ClearModifiedTargets(ID3D11DeviceContext *Ctx, EShadingModel Model)
    {
        const float ClearColor[4] = {0, 0, 0, 0};
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

    void BindBaseDrawTargets(ID3D11DeviceContext *Ctx, EShadingModel Model, ID3D11DepthStencilView *DSV)
    {
        ID3D11RenderTargetView *Targets[3] = {
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

    void BindDecalTargets(ID3D11DeviceContext *Ctx, EShadingModel Model, ID3D11DepthStencilView *DSV)
    {
        ID3D11RenderTargetView *Targets[3] = {
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

    ID3D11ShaderResourceView *GetSRV(ESurfaceSlot Slot) const
    {
        return Surfaces[static_cast<uint32>(Slot)].SRV;
    }

    ID3D11RenderTargetView *GetRTV(ESurfaceSlot Slot) const
    {
        return Surfaces[static_cast<uint32>(Slot)].RTV;
    }

  private:
    bool CreateSurface(ID3D11Device *Device, ESurfaceSlot Slot, DXGI_FORMAT Format, uint32 InWidth, uint32 InHeight)
    {
        FSurfaceTexture &Surface = Surfaces[static_cast<uint32>(Slot)];
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

    void ReleaseSurface(FSurfaceTexture &Surface)
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

  private:
    FSurfaceTexture Surfaces[static_cast<uint32>(ESurfaceSlot::Count)] = {};
    uint32 Width = 0;
    uint32 Height = 0;
};

/*
	공용 Constant Buffer + System Sampler를 관리하는 구조체입니다.
	모든 커맨드가 공통으로 사용하는 Frame/PerObject CB만 소유합니다.
	타입별 CB(Gizmo, Editor, Outline 등)는 FConstantBufferPool에서 관리됩니다.
*/

struct FRenderResources
{
	FConstantBuffer FrameBuffer;				// b0 — ECBSlot::Frame
	FConstantBuffer PerObjectConstantBuffer;	// b1 — ECBSlot::PerObject

	// System Samplers — 프레임 시작 시 s0-s2에 영구 바인딩
	ID3D11SamplerState* LinearClampSampler = nullptr;	// s0
	ID3D11SamplerState* LinearWrapSampler  = nullptr;	// s1
	ID3D11SamplerState* PointClampSampler  = nullptr;	// s2

	void Create(ID3D11Device* InDevice);
	void Release();

	// s0-s2 시스템 샘플러 일괄 바인딩 (프레임 1회)
	void BindSystemSamplers(ID3D11DeviceContext* Ctx);
};