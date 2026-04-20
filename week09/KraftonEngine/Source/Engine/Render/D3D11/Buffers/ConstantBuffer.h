#pragma once
#include "Render/Types/RenderTypes.h"
#include "Core/CoreTypes.h"
class FConstantBuffer
{
public:
    FConstantBuffer() = default;
    ~FConstantBuffer() { Release(); }
    FConstantBuffer(const FConstantBuffer&) = delete;
    FConstantBuffer& operator=(const FConstantBuffer&) = delete;
    FConstantBuffer(FConstantBuffer&&) noexcept;
    FConstantBuffer& operator=(FConstantBuffer&&) noexcept;
    void Create(ID3D11Device* InDevice, uint32 InByteWidth);
    void Release();
    void Update(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InByteWidth);
    ID3D11Buffer* GetBuffer();
private:
    ID3D11Buffer* Buffer = nullptr;
};
