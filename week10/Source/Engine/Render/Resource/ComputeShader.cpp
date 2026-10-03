#include "ComputeShader.h"

void FComputeShader::Release()
{
    if (CS)
    {
        CS->Release();
        CS = nullptr;
    }
}

void FComputeShader::Bind(ID3D11DeviceContext* Context)
{
    Context->CSSetShader(CS, nullptr, 0);
}

void FComputeShader::Unbind(ID3D11DeviceContext* Context)
{
    Context->CSSetShader(nullptr, nullptr, 0);
}

void FComputeShader::Dispatch(ID3D11DeviceContext* Context, uint32 X, uint32 Y, uint32 Z)
{
    Context->Dispatch(X, Y, Z);
}