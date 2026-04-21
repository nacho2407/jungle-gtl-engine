#include "Render/Passes/Base/PipelineStateTypes.h"
#include "Render/RHI/D3D11/State/RasterizerStateManager.h"

#define SAFE_RELEASE(Obj) \
    if (Obj)              \
    {                     \
        Obj->Release();   \
        Obj = nullptr;    \
    }

void FRasterizerStateManager::Create(ID3D11Device* InDevice)
{
    D3D11_RASTERIZER_DESC Desc = {};
    Desc.FillMode = D3D11_FILL_SOLID;
    Desc.CullMode = D3D11_CULL_BACK;
    InDevice->CreateRasterizerState(&Desc, &BackCull);

    Desc.CullMode = D3D11_CULL_FRONT;
    InDevice->CreateRasterizerState(&Desc, &FrontCull);

    Desc.CullMode = D3D11_CULL_NONE;
    InDevice->CreateRasterizerState(&Desc, &NoCull);

    Desc.FillMode = D3D11_FILL_WIREFRAME;
    Desc.CullMode = D3D11_CULL_NONE;
    InDevice->CreateRasterizerState(&Desc, &WireFrame);

    CurrentState = ERasterizerState::SolidBackCull;
}

void FRasterizerStateManager::Release()
{
    SAFE_RELEASE(BackCull);
    SAFE_RELEASE(FrontCull);
    SAFE_RELEASE(NoCull);
    SAFE_RELEASE(WireFrame);
}

void FRasterizerStateManager::Set(ID3D11DeviceContext* InContext, ERasterizerState InState)
{
    if (CurrentState == InState)
        return;

    switch (InState)
    {
    case ERasterizerState::SolidBackCull:
        InContext->RSSetState(BackCull);
        break;
    case ERasterizerState::SolidFrontCull:
        InContext->RSSetState(FrontCull);
        break;
    case ERasterizerState::SolidNoCull:
        InContext->RSSetState(NoCull);
        break;
    case ERasterizerState::WireFrame:
        InContext->RSSetState(WireFrame);
        break;
    }

    CurrentState = InState;
}
