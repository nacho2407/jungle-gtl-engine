#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/D3D11/Shaders/Stages/VertexShaderStage.h"
#include "Render/D3D11/Shaders/Stages/PixelShaderStage.h"

struct FMaterialParameterInfo;

class FShader
{
public:
    FShader() = default;
    ~FShader() { Release(); }

    FShader(const FShader&) = delete;
    FShader& operator=(const FShader&) = delete;
    FShader(FShader&& Other) noexcept;
    FShader& operator=(FShader&& Other) noexcept;

    void Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
                const D3D_SHADER_MACRO* InDefines = nullptr);
    void Release();
    void Bind(ID3D11DeviceContext* InDeviceContext) const;
    const TMap<FString, FMaterialParameterInfo*>& GetParameterLayout() const { return ShaderParameterLayout; }

    ID3D11VertexShader* GetVertexShader() const { return VertexShader.Get(); }
    ID3D11PixelShader* GetPixelShader() const { return PixelShader.Get(); }

private:
    void CreateInputLayoutFromReflection(ID3D11Device* InDevice, ID3DBlob* VSBlob);
    void ExtractCBufferInfo(ID3DBlob* ShaderBlob, TMap<FString, FMaterialParameterInfo*>& OutLayout);

private:
    FVertexShaderStage VertexShader;
    FPixelShaderStage PixelShader;
    ID3D11InputLayout* InputLayout = nullptr;
    TMap<FString, FMaterialParameterInfo*> ShaderParameterLayout;
};

using FGraphicsShaderProgram = FShader;
