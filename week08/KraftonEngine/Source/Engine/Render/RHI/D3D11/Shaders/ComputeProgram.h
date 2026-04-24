// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/RHI/D3D11/Shaders/ComputeShaderStage.h"

// FComputeProgram는 셰이더 컴파일 결과와 GPU 바인딩을 관리합니다.
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
