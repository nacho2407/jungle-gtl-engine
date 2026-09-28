#pragma once
#include <d3d11.h>
#include "Render/Common/ComPtr.h"

struct FDepthStencilResource
{
    TComPtr<ID3D11Texture2D> Texture;
    TComPtr<ID3D11DepthStencilView> DSV;
    TComPtr<ID3D11ShaderResourceView> SRV;
};
