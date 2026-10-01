// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/RHI/D3D11/Shaders/ComputeShaderStage.h"

/*
    새 compute shader COM 객체를 스테이지 래퍼에 설정합니다.
    기존 객체는 먼저 해제합니다.
*/
void FComputeShaderStage::Set(ID3D11ComputeShader* InShader, size_t InByteSize)
{
    Release();
    Shader   = InShader;
    ByteSize = InByteSize;
}

/*
    보유 중인 compute shader COM 객체를 해제합니다.
*/
void FComputeShaderStage::Release()
{
    if (Shader)
    {
        Shader->Release();
        Shader = nullptr;
    }
    ByteSize = 0;
}

/*
    compute shader stage 소유권을 다른 래퍼에서 이동합니다.
*/
FComputeShaderStage::FComputeShaderStage(FComputeShaderStage&& Other) noexcept
    : Shader(Other.Shader), ByteSize(Other.ByteSize)
{
    Other.Shader   = nullptr;
    Other.ByteSize = 0;
}

/*
    compute shader stage 소유권을 대입 이동합니다.
*/
FComputeShaderStage& FComputeShaderStage::operator=(FComputeShaderStage&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Shader         = Other.Shader;
        ByteSize       = Other.ByteSize;
        Other.Shader   = nullptr;
        Other.ByteSize = 0;
    }
    return *this;
}
