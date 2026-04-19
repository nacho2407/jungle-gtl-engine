#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"

class FBlendStateManager
{
public:
    void Create(ID3D11Device* InDevice);
    void Release();
    void Set(ID3D11DeviceContext* InContext, EBlendState InState);
    void ResetCache() { CurrentState = static_cast<EBlendState>(-1); }

private:
    ID3D11BlendState* Alpha = nullptr;
    ID3D11BlendState* Additive = nullptr;
    ID3D11BlendState* NoColorWrite = nullptr;

    EBlendState CurrentState = EBlendState::Opaque;
};
