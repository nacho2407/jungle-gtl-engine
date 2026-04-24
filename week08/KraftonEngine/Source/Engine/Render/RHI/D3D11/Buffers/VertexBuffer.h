// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/CoreTypes.h"
// FVertexBuffer는 GPU 버퍼 리소스의 생성과 바인딩을 관리합니다.
class FVertexBuffer
{
public:
    FVertexBuffer() = default;
    ~FVertexBuffer() { Release(); }
    FVertexBuffer(const FVertexBuffer&)            = delete;
    FVertexBuffer& operator=(const FVertexBuffer&) = delete;
    FVertexBuffer(FVertexBuffer&&) noexcept;
    FVertexBuffer& operator=(FVertexBuffer&&) noexcept;
    void           Create(ID3D11Device* InDevice, const void* InData, uint32 InVertexCount, uint32 InByteWidth, uint32 InStride);
    void           Release();
    uint32         GetVertexCount() const { return VertexCount; }
    uint32         GetStride() const { return Stride; }
    ID3D11Buffer*  GetBuffer() const;

private:
    ID3D11Buffer* Buffer      = nullptr;
    uint32        VertexCount = 0;
    uint32        Stride      = 0;
};
