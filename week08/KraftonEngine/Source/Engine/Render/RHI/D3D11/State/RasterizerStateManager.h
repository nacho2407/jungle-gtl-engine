#pragma once

#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Passes/Base/PipelineStateTypes.h"

class FRasterizerStateManager
{
public:
    void Create(ID3D11Device* InDevice);
    void Release();
    void Set(ID3D11DeviceContext* InContext, ERasterizerState InState);
    void ResetCache() { CurrentState = static_cast<ERasterizerState>(-1); }

private:
    ID3D11RasterizerState* BackCull = nullptr;
    ID3D11RasterizerState* FrontCull = nullptr;
    ID3D11RasterizerState* NoCull = nullptr;
    ID3D11RasterizerState* WireFrame = nullptr;

    ERasterizerState CurrentState = ERasterizerState::SolidBackCull;
};
