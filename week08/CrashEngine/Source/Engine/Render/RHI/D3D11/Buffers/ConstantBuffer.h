// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/CoreTypes.h"
// FConstantBuffer는 GPU 버퍼 리소스의 생성과 바인딩을 관리합니다.
class FConstantBuffer
{
public:
    FConstantBuffer() = default;
    ~FConstantBuffer() { Release(); }
    FConstantBuffer(const FConstantBuffer&)            = delete;
    FConstantBuffer& operator=(const FConstantBuffer&) = delete;
    FConstantBuffer(FConstantBuffer&&) noexcept;
    FConstantBuffer& operator=(FConstantBuffer&&) noexcept;
    void             Create(ID3D11Device* InDevice, uint32 InByteWidth);
    void             Release();
    void             Update(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InByteWidth);
    ID3D11Buffer*    GetBuffer();

private:
    ID3D11Buffer* Buffer = nullptr;
};
