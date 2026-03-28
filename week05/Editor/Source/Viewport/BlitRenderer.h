#pragma once

#include "ViewportTypes.h"
#include <d3d11.h>

class FBlitRenderer
{
public:
    FBlitRenderer() = default;
    ~FBlitRenderer();

    FBlitRenderer(const FBlitRenderer&) = delete;
    FBlitRenderer& operator=(const FBlitRenderer&) = delete;

    void Initialize(ID3D11Device* Device);
    void Release();

    void BlitAll(ID3D11DeviceContext* Context, const TArray<FViewportEntry>& Entries);

private:
    ID3D11VertexShader*      BlitVS          = nullptr;
    ID3D11PixelShader*       BlitPS          = nullptr;
    ID3D11SamplerState*      Sampler         = nullptr;
    ID3D11DepthStencilState* NoDepthState    = nullptr;
    ID3D11RasterizerState*   RasterizerState = nullptr;

    bool bInitialized = false;
};
