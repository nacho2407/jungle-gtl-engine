#pragma once

#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/RHI/D3D11/Shaders/VertexShaderStage.h"
#include "Render/RHI/D3D11/Shaders/PixelShaderStage.h"

/*
    D3D11 그래픽 셰이더 프로그램입니다.
    VS는 항상 필요하고 PS는 depth-only 같은 VS-only 패스를 위해 선택적으로 바인딩합니다.
*/
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
