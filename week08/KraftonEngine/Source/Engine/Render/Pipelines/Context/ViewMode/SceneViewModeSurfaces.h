#pragma once

#include "Core/CoreTypes.h"
#include "Render/Passes/Scene/ShadingTypes.h"
#include "Render/RHI/D3D11/Textures/SurfaceTexture.h"

/*
    뷰 모드 파이프라인이 사용하는 중간 표면 슬롯입니다.
    BaseDraw, Decal, Lighting/Resolve 패스가 같은 슬롯 의미를 공유하도록 고정된 레이아웃을 제공합니다.
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
    씬 뷰모드 파이프라인에서 사용하는 중간 표면 묶음입니다.
    BaseDraw는 기본 표면에 기록하고, Decal은 Modified 표면에 기록하며, Lighting/Resolve는 이를 SRV로 읽습니다.
*/
class FSceneViewModeSurfaces
{
public:
    bool Initialize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    void Resize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);
    void Release();

    void ClearBaseTargets(ID3D11DeviceContext* Ctx, EShadingModel Model);
    void ClearModifiedTargets(ID3D11DeviceContext* Ctx, EShadingModel Model);

    void BindBaseDrawTargets(ID3D11DeviceContext* Ctx, EShadingModel Model, ID3D11DepthStencilView* DSV);
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
