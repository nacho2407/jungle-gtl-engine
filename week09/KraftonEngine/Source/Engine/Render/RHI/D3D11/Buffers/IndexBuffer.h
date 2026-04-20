#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/CoreTypes.h"
class FIndexBuffer
{
public:
    FIndexBuffer() = default;
    ~FIndexBuffer() { Release(); }
    FIndexBuffer(const FIndexBuffer&) = delete;
    FIndexBuffer& operator=(const FIndexBuffer&) = delete;
    FIndexBuffer(FIndexBuffer&&) noexcept;
    FIndexBuffer& operator=(FIndexBuffer&&) noexcept;
    void Create(ID3D11Device* InDevice, const void* InData, uint32 InIndexCount, uint32 InByteWidth);
    void Release();
    uint32 GetIndexCount() const { return IndexCount; }
    ID3D11Buffer* GetBuffer() const;

private:
    ID3D11Buffer* Buffer = nullptr;
    uint32 IndexCount = 0;
};
