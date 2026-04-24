// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Render/RHI/D3D11/Common/D3D11API.h"

// FComputeShaderStage는 셰이더 컴파일 결과와 GPU 바인딩을 관리합니다.
class FComputeShaderStage
{
public:
    FComputeShaderStage() = default;
    ~FComputeShaderStage() { Release(); }
    FComputeShaderStage(const FComputeShaderStage&)            = delete;
    FComputeShaderStage& operator=(const FComputeShaderStage&) = delete;
    FComputeShaderStage(FComputeShaderStage&& Other) noexcept;
    FComputeShaderStage& operator=(FComputeShaderStage&& Other) noexcept;
    void                 Set(ID3D11ComputeShader* InShader, size_t InByteSize);
    void                 Release();
    ID3D11ComputeShader* Get() const { return Shader; }
    size_t               GetByteSize() const { return ByteSize; }

private:
    ID3D11ComputeShader* Shader   = nullptr;
    size_t               ByteSize = 0;
};
