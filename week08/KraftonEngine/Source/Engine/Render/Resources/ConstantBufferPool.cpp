#include "ConstantBufferPool.h"

void FConstantBufferPool::Initialize(ID3D11Device* InDevice)
{
    Device = InDevice;
}

void FConstantBufferPool::Release()
{
    for (auto& [Slot, CB] : Pool)
    {
        CB.Release();
    }
    Pool.clear();
    Device = nullptr;
}

FConstantBuffer* FConstantBufferPool::GetBuffer(uint32 Slot, uint32 ByteWidth)
{
    auto It = Pool.find(Slot);
    if (It != Pool.end())
    {
        return &It->second;
    }

    auto& CB = Pool[Slot];
    if (Device)
    {
        CB.Create(Device, ByteWidth);
    }
    return &CB;
}

FConstantBuffer* FConstantBufferPool::GetCBuffer(uint32 Index, uint32 ByteWidth)
{
    if (Index >= ConstantBuffers.size() || Index == -1)
    {
        return nullptr;
    }
    return &ConstantBuffers[Index];
}
