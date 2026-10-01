#pragma once

#include "Core/CoreTypes.h"
#include "Render/Execute/Context/ViewMode/ShadingModel.h"
#include "Render/RHI/D3D11/Textures/SurfaceTexture.h"

// EViewModeSurfaceslot는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class EViewModeSurfaceslot : uint8
{
    BaseColor = 0,
    Surface1,
    Surface2,
    ModifiedBaseColor,
    ModifiedSurface1,
    ModifiedSurface2,
    Count
};

// FViewModeSurfaces는 카메라와 화면 출력에 필요한 상태를 다룹니다.
class FViewModeSurfaces
{
public:
    bool Initialize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    void Resize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    void Release();

    void ClearBaseTargets(ID3D11DeviceContext* Ctx, EShadingModel Model);
    void ClearModifiedTargets(ID3D11DeviceContext* Ctx, EShadingModel Model);

    void BindOpaqueTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV);
    void BindDecalTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV);

    ID3D11ShaderResourceView* GetSRV(EViewModeSurfaceslot Slot) const;
    ID3D11RenderTargetView*   GetRTV(EViewModeSurfaceslot Slot) const;

private:
    bool CreateSurface(ID3D11Device* Device, EViewModeSurfaceslot Slot, DXGI_FORMAT Format, uint32 InWidth, uint32 InHeight);
    void ReleaseSurface(FSurfaceTexture& Surface);

private:
    FSurfaceTexture Surfaces[static_cast<uint32>(EViewModeSurfaceslot::Count)] = {};
    uint32          Width                                                      = 0;
    uint32          Height                                                     = 0;
};
