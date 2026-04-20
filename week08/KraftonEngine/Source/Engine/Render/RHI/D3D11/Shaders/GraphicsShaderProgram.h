#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/RHI/D3D11/Shaders/VertexShaderStage.h"
#include "Render/RHI/D3D11/Shaders/PixelShaderStage.h"

#include <string>
#include <unordered_set>

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

    bool Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
                const D3D_SHADER_MACRO* InDefines = nullptr);
    void Release();
    void Bind(ID3D11DeviceContext* InDeviceContext) const;
    const TMap<FString, FMaterialParameterInfo*>& GetParameterLayout() const { return ShaderParameterLayout; }

    ID3D11VertexShader* GetVertexShader() const { return VertexShader.Get(); }
    ID3D11PixelShader* GetPixelShader() const { return PixelShader.Get(); }
    bool IsValid() const { return VertexShader.Get() != nullptr && PixelShader.Get() != nullptr; }

private:
    bool CompileShaderStage(ID3DBlob** OutShaderBlob, const wchar_t* InFilePath, const char* InEntryPoint, const char* InTarget,
                            const D3D_SHADER_MACRO* InDefines, std::unordered_set<std::wstring>& OutDependencies, const char* InErrorTitle) const;
    bool CreateInputLayoutFromReflection(ID3D11Device* InDevice, ID3DBlob* VSBlob, ID3D11InputLayout** OutInputLayout) const;
    void ExtractCBufferInfo(ID3DBlob* ShaderBlob, TMap<FString, FMaterialParameterInfo*>& OutLayout) const;
    void ReleaseParameterLayout();

private:
    FVertexShaderStage VertexShader;
    FPixelShaderStage PixelShader;
    ID3D11InputLayout* InputLayout = nullptr;
    TMap<FString, FMaterialParameterInfo*> ShaderParameterLayout;
};

using FGraphicsShaderProgram = FShader;
