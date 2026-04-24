// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/CoreTypes.h"
// FDynamicVertexBuffer는 GPU 버퍼 리소스의 생성과 바인딩을 관리합니다.
class FDynamicVertexBuffer
{
public:
    FDynamicVertexBuffer() = default;
    ~FDynamicVertexBuffer() { Release(); }
    FDynamicVertexBuffer(const FDynamicVertexBuffer&)            = delete;
    FDynamicVertexBuffer& operator=(const FDynamicVertexBuffer&) = delete;
    void                  Create(ID3D11Device* InDevice, uint32 InMaxCount, uint32 InStride);
    void                  Release();
    void                  EnsureCapacity(ID3D11Device* InDevice, uint32 RequiredCount);
    bool                  Update(ID3D11DeviceContext* Context, const void* Data, uint32 Count);
    void                  Bind(ID3D11DeviceContext* Context, uint32 Slot = 0);
    uint32                GetMaxCount() const { return MaxCount; }
    ID3D11Buffer*         GetBuffer() const { return Buffer; }
    uint32                GetStride() const { return Stride; }

private:
    ID3D11Buffer* Buffer   = nullptr;
    uint32        MaxCount = 0;
    uint32        Stride   = 0;
};
