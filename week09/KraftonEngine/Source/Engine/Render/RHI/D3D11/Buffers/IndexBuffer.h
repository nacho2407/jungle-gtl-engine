// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/CoreTypes.h"
// FIndexBuffer는 GPU 버퍼 리소스의 생성과 바인딩을 관리합니다.
class FIndexBuffer
{
public:
    FIndexBuffer() = default;
    ~FIndexBuffer() { Release(); }
    FIndexBuffer(const FIndexBuffer&)            = delete;
    FIndexBuffer& operator=(const FIndexBuffer&) = delete;
    FIndexBuffer(FIndexBuffer&&) noexcept;
    FIndexBuffer& operator=(FIndexBuffer&&) noexcept;
    void          Create(ID3D11Device* InDevice, const void* InData, uint32 InIndexCount, uint32 InByteWidth);
    void          Release();
    uint32        GetIndexCount() const { return IndexCount; }
    ID3D11Buffer* GetBuffer() const;

private:
    ID3D11Buffer* Buffer     = nullptr;
    uint32        IndexCount = 0;
};
