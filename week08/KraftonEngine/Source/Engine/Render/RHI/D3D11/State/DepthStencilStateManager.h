// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Resources/State/RenderStateTypes.h"

// FDepthStencilStateManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FDepthStencilStateManager
{
public:
    void Create(ID3D11Device* InDevice);
    void Release();
    void Set(ID3D11DeviceContext* InContext, EDepthStencilState InState);
    void ResetCache() { CurrentState = static_cast<EDepthStencilState>(-1); }

private:
    ID3D11DepthStencilState* Default          = nullptr;
    ID3D11DepthStencilState* DepthReadOnly    = nullptr;
    ID3D11DepthStencilState* StencilWrite     = nullptr;
    ID3D11DepthStencilState* StencilMaskEqual = nullptr;
    ID3D11DepthStencilState* NoDepth          = nullptr;
    ID3D11DepthStencilState* GizmoInside      = nullptr;
    ID3D11DepthStencilState* GizmoOutside     = nullptr;

    EDepthStencilState CurrentState = static_cast<EDepthStencilState>(-1);
};
