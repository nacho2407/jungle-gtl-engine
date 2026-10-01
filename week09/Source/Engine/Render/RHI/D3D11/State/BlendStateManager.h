#pragma once

#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Resources/State/RenderStateTypes.h"

// FBlendStateManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FBlendStateManager
{
public:
    void Create(ID3D11Device* InDevice);
    void Release();
    void Set(ID3D11DeviceContext* InContext, EBlendState InState);
    void ResetCache() { CurrentState = static_cast<EBlendState>(-1); }

private:
    ID3D11BlendState* Alpha        = nullptr;
    ID3D11BlendState* Additive     = nullptr;
    ID3D11BlendState* NoColorWrite = nullptr;

    EBlendState CurrentState = EBlendState::Opaque;
};
