#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

/*
    패스/프록시가 자주 요청하는 상수 버퍼를 슬롯 키 기준으로 재사용하는 풀입니다.
*/
class FConstantBufferPool : public TSingleton<FConstantBufferPool>
{
    friend class TSingleton<FConstantBufferPool>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();

    FConstantBuffer* GetBuffer(uint32 Slot, uint32 ByteWidth);
    FConstantBuffer* GetCBuffer(uint32 Index, uint32 ByteWidth);

private:
    FConstantBufferPool() = default;

    ID3D11Device* Device = nullptr;
    TMap<uint32, FConstantBuffer> Pool;
    TArray<FConstantBuffer> ConstantBuffers;
};
