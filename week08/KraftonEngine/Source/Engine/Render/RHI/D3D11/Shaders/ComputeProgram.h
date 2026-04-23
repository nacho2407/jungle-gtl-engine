#pragma once

#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/RHI/D3D11/Shaders/ComputeShaderStage.h"

/*
    D3D11 컴퓨트 셰이더 프로그램입니다.
    CS 하나만 소유하며 그래픽 파이프라인 상태와 독립적으로 바인딩됩니다.
*/
class FComputeProgram : public FShaderProgramBase
{
public:
    FComputeProgram() = default;
    ~FComputeProgram() override { Release(); }

    FComputeProgram(const FComputeProgram&)            = delete;
    FComputeProgram& operator=(const FComputeProgram&) = delete;
    FComputeProgram(FComputeProgram&& Other) noexcept;
    FComputeProgram& operator=(FComputeProgram&& Other) noexcept;

    bool Create(ID3D11Device* InDevice, const FComputeProgramDesc& InDesc);

    void Release() override;
    void Bind(ID3D11DeviceContext* InDeviceContext) const override;
    bool IsValid() const override;

    ID3D11ComputeShader* GetComputeShader() const { return ComputeShader.Get(); }

private:
    bool CompileComputeShader(
        ID3D11Device*                     InDevice,
        const FShaderStageDesc&           InDesc,
        std::unordered_set<std::wstring>& OutDependencies);

private:
    FComputeShaderStage ComputeShader;
};
