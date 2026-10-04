#pragma once

#include "Core/CoreTypes.h"
#include <d3d11.h>

class FComputeShader
{
public:
    void Release();

    void Bind(ID3D11DeviceContext* Context);
    void Unbind(ID3D11DeviceContext* Context);

    void Dispatch(ID3D11DeviceContext* Context, uint32 X, uint32 Y, uint32 Z);

    ID3D11ComputeShader* CS = nullptr;
};