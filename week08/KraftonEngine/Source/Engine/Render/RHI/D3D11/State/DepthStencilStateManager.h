#pragma once

#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Passes/Base/PipelineStateTypes.h"

class FDepthStencilStateManager
{
public:
    void Create(ID3D11Device* InDevice);
    void Release();
    void Set(ID3D11DeviceContext* InContext, EDepthStencilState InState);
    void ResetCache() { CurrentState = static_cast<EDepthStencilState>(-1); }

private:
    ID3D11DepthStencilState* Default = nullptr;
    ID3D11DepthStencilState* DepthReadOnly = nullptr;
    ID3D11DepthStencilState* StencilWrite = nullptr;
    ID3D11DepthStencilState* StencilMaskEqual = nullptr;
    ID3D11DepthStencilState* NoDepth = nullptr;
    ID3D11DepthStencilState* GizmoInside = nullptr;
    ID3D11DepthStencilState* GizmoOutside = nullptr;

    // 무효 값으로 초기화 — 첫 Set() 호출이 반드시 GPU에 도달하도록 보장
    EDepthStencilState CurrentState = static_cast<EDepthStencilState>(-1);
};
