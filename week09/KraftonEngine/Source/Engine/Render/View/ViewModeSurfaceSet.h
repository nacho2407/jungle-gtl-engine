#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/ShadingTypes.h"
#include "Render/D3D11/Buffers/Buffers.h"

struct FSurfaceTexture
{
    ID3D11Texture2D* Texture = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;
    ID3D11ShaderResourceView* SRV = nullptr;
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
    bool Initialize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    void Resize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    void Release();

    void ClearBaseTargets(ID3D11DeviceContext* Ctx, EShadingModel Model);
    void ClearModifiedTargets(ID3D11DeviceContext* Ctx, EShadingModel Model);

    void BindBaseDrawTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV);
    void BindDecalTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV);

    ID3D11ShaderResourceView* GetSRV(ESurfaceSlot Slot) const;
    ID3D11RenderTargetView* GetRTV(ESurfaceSlot Slot) const;

private:
    bool CreateSurface(ID3D11Device* Device, ESurfaceSlot Slot, DXGI_FORMAT Format, uint32 InWidth, uint32 InHeight);
    void ReleaseSurface(FSurfaceTexture& Surface);

private:
    FSurfaceTexture Surfaces[static_cast<uint32>(ESurfaceSlot::Count)] = {};
    uint32 Width = 0;
    uint32 Height = 0;
};
