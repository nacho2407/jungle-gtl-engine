// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/RHI/D3D11/State/BlendStateManager.h"

#define SAFE_RELEASE(Obj) \
    if (Obj)              \
    {                     \
        Obj->Release();   \
        Obj = nullptr;    \
    }

void FBlendStateManager::Create(ID3D11Device* InDevice)
{
    // Alpha Blend
    D3D11_BLEND_DESC Desc                      = {};
    Desc.RenderTarget[0].BlendEnable           = TRUE;
    Desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    Desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    Desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    Desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA;
    Desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    Desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    InDevice->CreateBlendState(&Desc, &Alpha);

    // Additive: RGB = Src + Dest.
    Desc                                       = {};
    Desc.AlphaToCoverageEnable                 = FALSE;
    Desc.IndependentBlendEnable                = FALSE;
    Desc.RenderTarget[0].BlendEnable           = TRUE;
    Desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    Desc.RenderTarget[0].DestBlend             = D3D11_BLEND_ONE;
    Desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    Desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    Desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    Desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    InDevice->CreateBlendState(&Desc, &Additive);

    // No Color Write
    Desc                                       = {};
    Desc.AlphaToCoverageEnable                 = FALSE;
    Desc.IndependentBlendEnable                = FALSE;
    Desc.RenderTarget[0].BlendEnable           = FALSE;
    Desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    Desc.RenderTarget[0].DestBlend             = D3D11_BLEND_ZERO;
    Desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    Desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    Desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    Desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    Desc.RenderTarget[0].RenderTargetWriteMask = 0;
    InDevice->CreateBlendState(&Desc, &NoColorWrite);
}

void FBlendStateManager::Release()
{
    SAFE_RELEASE(Alpha);
    SAFE_RELEASE(Additive);
    SAFE_RELEASE(NoColorWrite);
}

void FBlendStateManager::Set(ID3D11DeviceContext* InContext, EBlendState InState)
{
    if (CurrentState == InState)
        return;

    const float BlendFactor[4] = { 0, 0, 0, 0 };

    switch (InState)
    {
    case EBlendState::Opaque:
        InContext->OMSetBlendState(nullptr, BlendFactor, 0xffffffff);
        break;
    case EBlendState::AlphaBlend:
        InContext->OMSetBlendState(Alpha, BlendFactor, 0xffffffff);
        break;
    case EBlendState::Additive:
        InContext->OMSetBlendState(Additive, BlendFactor, 0xffffffff);
        break;
    case EBlendState::NoColor:
        InContext->OMSetBlendState(NoColorWrite, BlendFactor, 0xFFFFFFFF);
        break;
    }

    CurrentState = InState;
}
