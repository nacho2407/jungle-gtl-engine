#pragma once
#include "Render/Types/RenderTypes.h"
#include "Core/CoreTypes.h"
class FDynamicIndexBuffer
{
public:
    FDynamicIndexBuffer() = default;
    ~FDynamicIndexBuffer() { Release(); }
    FDynamicIndexBuffer(const FDynamicIndexBuffer&) = delete;
    FDynamicIndexBuffer& operator=(const FDynamicIndexBuffer&) = delete;
    void Create(ID3D11Device* InDevice, uint32 InMaxCount);
    void Release();
    void EnsureCapacity(ID3D11Device* InDevice, uint32 RequiredCount);
    bool Update(ID3D11DeviceContext* Context, const void* Data, uint32 Count);
    void Bind(ID3D11DeviceContext* Context);
    uint32 GetMaxCount() const { return MaxCount; }
    ID3D11Buffer* GetBuffer() const { return Buffer; }
private:
    ID3D11Buffer* Buffer = nullptr;
    uint32 MaxCount = 0;
};
