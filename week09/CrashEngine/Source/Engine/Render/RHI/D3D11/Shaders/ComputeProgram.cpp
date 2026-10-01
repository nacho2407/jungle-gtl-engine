// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/RHI/D3D11/Shaders/ComputeProgram.h"

#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
constexpr const char* GComputeShaderTarget = "cs_5_0";
}

/*
    compute program 소유 리소스를 이동 생성합니다.
*/
FComputeProgram::FComputeProgram(FComputeProgram&& Other) noexcept
    : FShaderProgramBase(std::move(Other)), ComputeShader(std::move(Other.ComputeShader))
{
}

/*
    compute program 소유 리소스를 대입 이동합니다.
*/
FComputeProgram& FComputeProgram::operator=(FComputeProgram&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        FShaderProgramBase::operator=(std::move(Other));
        ComputeShader = std::move(Other.ComputeShader);
    }
    return *this;
}

/*
    desc에 지정된 CS를 컴파일하여 compute program을 생성합니다.
*/
bool FComputeProgram::Create(ID3D11Device* InDevice, const FComputeProgramDesc& InDesc)
{
    if (InDevice == nullptr || InDesc.CS.FilePath.empty() || InDesc.CS.EntryPoint.empty())
    {
        return false;
    }

    std::unordered_set<std::wstring> Dependencies;
    return CompileComputeShader(InDevice, InDesc.CS, Dependencies);
}

/*
    compute shader stage를 컴파일하고 D3D11 compute shader 객체를 생성합니다.
*/
bool FComputeProgram::CompileComputeShader(
    ID3D11Device*                     InDevice,
    const FShaderStageDesc&           InDesc,
    std::unordered_set<std::wstring>& OutDependencies)
{
    ID3DBlob* ComputeShaderCSO = nullptr;
    if (!CompileShaderBlob(&ComputeShaderCSO, InDesc, GComputeShaderTarget, OutDependencies, "Compute Shader Compile Error"))
    {
        return false;
    }

    ID3D11ComputeShader* CS = nullptr;
    const HRESULT        Hr = InDevice->CreateComputeShader(ComputeShaderCSO->GetBufferPointer(), ComputeShaderCSO->GetBufferSize(), nullptr, &CS);
    if (FAILED(Hr))
    {
        std::cerr << "Failed to create Compute Shader (HRESULT: " << Hr << ")" << std::endl;
        ComputeShaderCSO->Release();
        return false;
    }

    TMap<FString, FMaterialParameterInfo*> NewShaderParameterLayout;
    ExtractCBufferInfo(ComputeShaderCSO, NewShaderParameterLayout);

    Release();
    ComputeShader.Set(CS, ComputeShaderCSO->GetBufferSize());
    ShaderParameterLayout = std::move(NewShaderParameterLayout);

    ComputeShaderCSO->Release();
    return true;
}

/*
    compute program의 D3D11 리소스를 해제합니다.
*/
void FComputeProgram::Release()
{
    ReleaseBase();
    ComputeShader.Release();
}

/*
    compute shader stage에 프로그램을 바인딩합니다.
*/
void FComputeProgram::Bind(ID3D11DeviceContext* InDeviceContext) const
{
    if (!InDeviceContext)
    {
        return;
    }

    InDeviceContext->CSSetShader(ComputeShader.Get(), nullptr, 0);
}

/*
    compute program이 제출 가능한 상태인지 반환합니다.
*/
bool FComputeProgram::IsValid() const
{
    return ComputeShader.Get() != nullptr;
}
