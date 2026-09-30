// 렌더 영역의 세부 동작을 구현합니다.
#include "ConstantBufferCache.h"

void FConstantBufferCache::Initialize(ID3D11Device* InDevice)
{
    Device = InDevice;
}

void FConstantBufferCache::Release()
{
    for (auto& [Slot, CB] : BuffersBySlot)
    {
        CB.Release();
    }

    BuffersBySlot.clear();
    Device = nullptr;
}

FConstantBuffer* FConstantBufferCache::GetOrCreate(uint32 Slot, uint32 ByteWidth)
{
    auto It = BuffersBySlot.find(Slot);
    if (It != BuffersBySlot.end())
    {
        return &It->second;
    }

    auto& CB = BuffersBySlot[Slot];
    if (Device)
    {
        CB.Create(Device, ByteWidth);
    }

    return &CB;
}

FConstantBuffer* FConstantBufferCache::Find(uint32 Slot)
{
    auto It = BuffersBySlot.find(Slot);
    if (It == BuffersBySlot.end())
    {
        return nullptr;
    }

    return &It->second;
}
