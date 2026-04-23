#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

/*
    슬롯 키 기준으로 상수 버퍼를 재사용하는 캐시입니다.

    이 타입은 상수 버퍼의 생성/조회/재사용만 담당합니다.
    렌더 정책을 결정하거나 여러 시스템의 생명주기를 조정하지 않으므로
    Manager보다는 Cache라는 이름이 더 적합합니다.
*/
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
