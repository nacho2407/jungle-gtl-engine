#pragma once

#include "Core/CoreTypes.h"
#include "Render/Execute/Passes/Scene/ShadingTypes.h"
#include "Render/RHI/D3D11/Textures/SurfaceTexture.h"

/*
    �?모드 ?�이?�라?�이 ?�용?�는 중간 ?�면 ?�롯?�니??
    Opaque, Decal, Lighting/Resolve ?�스가 같�? ?�롯 ?��?�?공유?�도�?고정???�이?�웃???�공?�니??
*/
enum class ESceneViewModeSurfaceSlot : uint8
{
    BaseColor = 0,
    Surface1,
    Surface2,
    ModifiedBaseColor,
    ModifiedSurface1,
    ModifiedSurface2,
    Count
};

/*
    ??뷰모???�이?�라?�에???�용?�는 중간 ?�면 묶음?�니??
    Opaque??기본 ?�면??기록?�고, Decal?� Modified ?�면??기록?�며, Lighting/Resolve???��? SRV�??�습?�다.
*/
class FSceneViewModeSurfaces
{
public:
    bool Initialize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    void Resize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    void Release();

    void ClearBaseTargets(ID3D11DeviceContext* Ctx, EShadingModel Model);
    void ClearModifiedTargets(ID3D11DeviceContext* Ctx, EShadingModel Model);

    void BindOpaqueTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV);
    void BindDecalTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV);

    ID3D11ShaderResourceView* GetSRV(ESceneViewModeSurfaceSlot Slot) const;
    ID3D11RenderTargetView* GetRTV(ESceneViewModeSurfaceSlot Slot) const;

private:
    bool CreateSurface(ID3D11Device* Device, ESceneViewModeSurfaceSlot Slot, DXGI_FORMAT Format, uint32 InWidth, uint32 InHeight);
    void ReleaseSurface(FSurfaceTexture& Surface);

private:
    FSurfaceTexture Surfaces[static_cast<uint32>(ESceneViewModeSurfaceSlot::Count)] = {};
    uint32 Width = 0;
    uint32 Height = 0;
};
