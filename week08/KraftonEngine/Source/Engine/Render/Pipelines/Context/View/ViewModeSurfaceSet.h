#pragma once

#include "Core/CoreTypes.h"
#include "Render/Pipelines/ShadingTypes.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

/*
    뷰 모드별 중간 렌더 타깃 1장을 표현하는 구조체입니다.
    각 슬롯은 RTV/SRV 쌍으로 생성되어 BaseDraw, Decal, Lighting 패스가 함께 사용합니다.
*/
struct FSurfaceTexture
{
    ID3D11Texture2D* Texture = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;
    ID3D11ShaderResourceView* SRV = nullptr;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

/*
    뷰 모드 전용 중간 표면 슬롯입니다.

    [BaseDraw 출력]
    - BaseColor          : 모든 뷰 모드의 기본 색
    - Surface1           : GouraudL / Normal
    - Surface2           : Blinn-Phong의 MaterialParam
    - ModifiedBaseColor  : 데칼 적용 후 BaseColor
    - ModifiedSurface1   : 데칼 적용 후 GouraudL/Normal
    - ModifiedSurface2   : 데칼 적용 후 MaterialParam

    [뷰 모드별 의미]
    - Gouraud
      BaseColor         = BaseColor
      Surface1          = GouraudL
      Surface2          = 미사용
      ModifiedBaseColor = 데칼 적용 BaseColor
      ModifiedSurface1  = 미사용
      ModifiedSurface2  = 미사용

    - Lambert
      BaseColor         = BaseColor
      Surface1          = Normal
      Surface2          = 미사용
      ModifiedBaseColor = 데칼 적용 BaseColor
      ModifiedSurface1  = 데칼 적용 Normal
      ModifiedSurface2  = 미사용

    - Blinn-Phong
      BaseColor         = BaseColor
      Surface1          = Normal
      Surface2          = MaterialParam
      ModifiedBaseColor = 데칼 적용 BaseColor
      ModifiedSurface1  = 데칼 적용 Normal
      ModifiedSurface2  = 데칼 적용 MaterialParam

    - Unlit
      BaseColor         = BaseColor
      Surface1          = 미사용
      Surface2          = 미사용
      ModifiedBaseColor = 데칼 적용 BaseColor
      ModifiedSurface1  = 미사용
      ModifiedSurface2  = 미사용
*/
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

/*
    뷰 모드 렌더링에 필요한 중간 표면 묶음입니다.
    BaseDraw는 기본 표면에, Decal은 Modified 표면에 기록하고 Lighting/Resolve가 이를 읽습니다.
*/
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
