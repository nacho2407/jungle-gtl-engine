#pragma once

#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/RHI/D3D11/Shaders/VertexShaderStage.h"
#include "Render/RHI/D3D11/Shaders/PixelShaderStage.h"

// FGraphicsProgram는 셰이더 컴파일 결과와 GPU 바인딩을 관리합니다.
class FGraphicsProgram : public FShaderProgramBase
{
public:
    FGraphicsProgram() = default;
    ~FGraphicsProgram() override { Release(); }

    FGraphicsProgram(const FGraphicsProgram&)            = delete;
    FGraphicsProgram& operator=(const FGraphicsProgram&) = delete;
    FGraphicsProgram(FGraphicsProgram&& Other) noexcept;
    FGraphicsProgram& operator=(FGraphicsProgram&& Other) noexcept;

    bool Create(ID3D11Device* InDevice, const FGraphicsProgramDesc& InDesc);

    void Release() override;
    void Bind(ID3D11DeviceContext* InDeviceContext) const override;
    bool IsValid() const override;

    ID3D11VertexShader* GetVertexShader() const { return VertexShader.Get(); }
    ID3D11PixelShader*  GetPixelShader() const { return PixelShader.Get(); }
    ID3D11InputLayout*  GetInputLayout() const { return InputLayout; }

private:
    bool CompileVertexShader(
        ID3D11Device*                     InDevice,
        const FShaderStageDesc&           InDesc,
        ID3DBlob**                        OutVSBlob,
        ID3D11VertexShader**              OutVS,
        std::unordered_set<std::wstring>& OutDependencies) const;

    bool CompilePixelShader(
        ID3D11Device*                     InDevice,
        const FShaderStageDesc&           InDesc,
        ID3DBlob**                        OutPSBlob,
        ID3D11PixelShader**               OutPS,
        std::unordered_set<std::wstring>& OutDependencies) const;

    bool CreateInputLayoutFromReflection(
        ID3D11Device*       InDevice,
        ID3DBlob*           InVSBlob,
        ID3D11InputLayout** OutInputLayout) const;

private:
    FVertexShaderStage VertexShader;
    FPixelShaderStage  PixelShader;
    ID3D11InputLayout* InputLayout = nullptr;
};
