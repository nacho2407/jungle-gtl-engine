// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/RHI/D3D11/Shaders/VertexShaderStage.h"

#include "Profiling/MemoryStats.h"

/*
    새 vertex shader COM 객체를 스테이지 래퍼에 설정합니다.
    기존 객체를 해제하고 bytecode 크기를 메모리 통계에 반영합니다.
*/
void FVertexShaderStage::Set(ID3D11VertexShader* InShader, size_t InByteSize)
{
    Release();
    Shader   = InShader;
    ByteSize = InByteSize;
    if (Shader)
    {
        MemoryStats::AddVertexShaderMemory(static_cast<uint32>(ByteSize));
    }
}

/*
    보유 중인 vertex shader COM 객체를 해제합니다.
    등록했던 bytecode 메모리 통계도 함께 차감합니다.
*/
void FVertexShaderStage::Release()
{
    if (Shader)
    {
        MemoryStats::SubVertexShaderMemory(static_cast<uint32>(ByteSize));
        Shader->Release();
        Shader = nullptr;
    }
    ByteSize = 0;
}

/*
    vertex shader stage 소유권을 다른 래퍼에서 이동합니다.
*/
FVertexShaderStage::FVertexShaderStage(FVertexShaderStage&& Other) noexcept
    : Shader(Other.Shader), ByteSize(Other.ByteSize)
{
    Other.Shader   = nullptr;
    Other.ByteSize = 0;
}

/*
    vertex shader stage 소유권을 대입 이동합니다.
*/
FVertexShaderStage& FVertexShaderStage::operator=(FVertexShaderStage&& Other) noexcept
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
