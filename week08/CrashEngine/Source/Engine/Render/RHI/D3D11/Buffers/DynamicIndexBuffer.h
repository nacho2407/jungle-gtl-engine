#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/CoreTypes.h"
// FDynamicIndexBuffer는 GPU 버퍼 리소스의 생성과 바인딩을 관리합니다.
class FDynamicIndexBuffer
{
public:
    FDynamicIndexBuffer() = default;
    ~FDynamicIndexBuffer() { Release(); }
    FDynamicIndexBuffer(const FDynamicIndexBuffer&)            = delete;
    FDynamicIndexBuffer& operator=(const FDynamicIndexBuffer&) = delete;
    void                 Create(ID3D11Device* InDevice, uint32 InMaxCount);
    void                 Release();
    void                 EnsureCapacity(ID3D11Device* InDevice, uint32 RequiredCount);
    bool                 Update(ID3D11DeviceContext* Context, const void* Data, uint32 Count);
    void                 Bind(ID3D11DeviceContext* Context);
    uint32               GetMaxCount() const { return MaxCount; }
    ID3D11Buffer*        GetBuffer() const { return Buffer; }

private:
    ID3D11Buffer* Buffer   = nullptr;
    uint32        MaxCount = 0;
};
