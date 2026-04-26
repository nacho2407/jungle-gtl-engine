#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

// FConstantBufferCache는 렌더 영역의 핵심 동작을 담당합니다.
class FConstantBufferCache : public TSingleton<FConstantBufferCache>
{
    friend class TSingleton<FConstantBufferCache>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();

    FConstantBuffer* GetOrCreate(uint32 Slot, uint32 ByteWidth);
    FConstantBuffer* GetBuffer(uint32 Slot, uint32 ByteWidth) { return GetOrCreate(Slot, ByteWidth); }
    FConstantBuffer* Find(uint32 Slot);

private:
    FConstantBufferCache() = default;

private:
    ID3D11Device*                 Device = nullptr;
    TMap<uint32, FConstantBuffer> BuffersBySlot;
};
