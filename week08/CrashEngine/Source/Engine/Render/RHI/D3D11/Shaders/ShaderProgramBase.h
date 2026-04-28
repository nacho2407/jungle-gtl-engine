#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Resources/Shaders/ShaderProgramDesc.h"

#include <string>
#include <unordered_set>

struct FMaterialParameterInfo;

// FShaderProgramBase는 셰이더 컴파일 결과와 GPU 바인딩을 관리합니다.
class FShaderProgramBase
{
public:
    FShaderProgramBase() = default;
    virtual ~FShaderProgramBase() { ReleaseBase(); }

    FShaderProgramBase(const FShaderProgramBase&)            = delete;
    FShaderProgramBase& operator=(const FShaderProgramBase&) = delete;
    FShaderProgramBase(FShaderProgramBase&& Other) noexcept;
    FShaderProgramBase& operator=(FShaderProgramBase&& Other) noexcept;

    virtual void Release()                                        = 0;
    virtual void Bind(ID3D11DeviceContext* InDeviceContext) const = 0;
    virtual bool IsValid() const                                  = 0;

    const TMap<FString, FMaterialParameterInfo*>& GetParameterLayout() const { return ShaderParameterLayout; }

    static bool CompileShaderBlobStandalone(
        ID3DBlob**                        OutShaderBlob,
        const FShaderStageDesc&           InDesc,
        const char*                       InTarget,
        const char*                       InErrorTitle,
        std::unordered_set<std::wstring>* OutDependencies = nullptr);

protected:
    bool CompileShaderBlob(
        ID3DBlob**                        OutShaderBlob,
        const FShaderStageDesc&           InDesc,
        const char*                       InTarget,
        std::unordered_set<std::wstring>& OutDependencies,
        const char*                       InErrorTitle) const;

    void ExtractCBufferInfo(
        ID3DBlob*                               InShaderBlob,
        TMap<FString, FMaterialParameterInfo*>& OutLayout) const;

    void ReleaseParameterLayout();
    void ReleaseBase();

protected:
    TMap<FString, FMaterialParameterInfo*> ShaderParameterLayout;
};
