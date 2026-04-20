#pragma once

#include "Core/CoreTypes.h"
class FConstantBuffer;
struct ID3D11ShaderResourceView;

struct FShaderBindingSet
{
    FConstantBuffer* PerObjectCB = nullptr;
    FConstantBuffer* PerShaderCB[2] = {};
    ID3D11ShaderResourceView* DiffuseSRV = nullptr;
    ID3D11ShaderResourceView* NormalSRV = nullptr;
};
