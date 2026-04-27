// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"

#include "Materials/Material.h"
#include "Platform/Paths.h"
#include "Render/Resources/Shaders/ShaderIncludeLoader.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace
{
/*
    셰이더 파일 경로를 엔진 루트 기준 절대 경로로 정규화합니다.
    include loader와 D3DCompileFromFile이 같은 기준 경로를 사용하도록 맞춥니다.
*/
std::wstring MakeAbsoluteShaderPath(const FString& InPath)
{
    std::filesystem::path Path = FPaths::ToPath(FPaths::ToWide(InPath));
    if (!Path.is_absolute())
    {
        Path = FPaths::ToPath(FPaths::RootDir()) / Path;
    }

    std::error_code EC;
    Path = std::filesystem::weakly_canonical(Path, EC);
    if (EC)
    {
        Path = Path.lexically_normal();
    }

    return Path.wstring();
}

/*
    desc의 compile define 목록을 D3D_SHADER_MACRO 배열로 변환합니다.
    D3D 컴파일러 요구사항에 맞춰 마지막 원소는 null terminator로 둡니다.
*/
void BuildD3DDefines(const TArray<FShaderCompileDefine>& InDefines, TArray<D3D_SHADER_MACRO>& OutDefines)
{
    OutDefines.clear();
    OutDefines.reserve(InDefines.size() + 1);

    for (const FShaderCompileDefine& Def : InDefines)
    {
        D3D_SHADER_MACRO Macro = {};
        Macro.Name             = _strdup(Def.Name.c_str());
        Macro.Definition       = _strdup(Def.Value.c_str());
        OutDefines.push_back(Macro);
    }

    OutDefines.push_back({ nullptr, nullptr });
}

/*
    BuildD3DDefines에서 복제한 문자열 메모리를 해제합니다.
*/
void ReleaseD3DDefines(TArray<D3D_SHADER_MACRO>& InOutDefines)
{
    for (D3D_SHADER_MACRO& Macro : InOutDefines)
    {
        if (Macro.Name)
        {
            free(const_cast<char*>(Macro.Name));
        }
        if (Macro.Definition)
        {
            free(const_cast<char*>(Macro.Definition));
        }
    }
    InOutDefines.clear();
}
} // namespace

/*
    공통 shader program 상태를 이동 생성합니다.
    파생 클래스의 stage 객체 이동은 각 파생 클래스가 따로 처리합니다.
*/
FShaderProgramBase::FShaderProgramBase(FShaderProgramBase&& Other) noexcept
    : ShaderParameterLayout(std::move(Other.ShaderParameterLayout))
{
    Other.ShaderParameterLayout.clear();
}

/*
    공통 shader program 상태를 대입 이동합니다.
    기존 리플렉션 파라미터 레이아웃은 먼저 해제합니다.
*/
FShaderProgramBase& FShaderProgramBase::operator=(FShaderProgramBase&& Other) noexcept
{
    if (this != &Other)
    {
        ReleaseBase();
        ShaderParameterLayout = std::move(Other.ShaderParameterLayout);
        Other.ShaderParameterLayout.clear();
    }
    return *this;
}

/*
    단일 shader stage를 bytecode blob으로 컴파일합니다.
    파일 경로 정규화, include 추적, compile define 변환을 이 함수에서 공통 처리합니다.
*/
bool FShaderProgramBase::CompileShaderBlob(
    ID3DBlob**                        OutShaderBlob,
    const FShaderStageDesc&           InDesc,
    const char*                       InTarget,
    std::unordered_set<std::wstring>& OutDependencies,
    const char*                       InErrorTitle) const
{
    return CompileShaderBlobStandalone(OutShaderBlob, InDesc, InTarget, InErrorTitle, &OutDependencies);
}

bool FShaderProgramBase::CompileShaderBlobStandalone(
    ID3DBlob**                        OutShaderBlob,
    const FShaderStageDesc&           InDesc,
    const char*                       InTarget,
    const char*                       InErrorTitle,
    std::unordered_set<std::wstring>* OutDependencies)
{
    if (OutShaderBlob == nullptr || InDesc.FilePath.empty() || InDesc.EntryPoint.empty() || InTarget == nullptr)
    {
        return false;
    }

    *OutShaderBlob = nullptr;

    const std::wstring AbsolutePath = MakeAbsoluteShaderPath(InDesc.FilePath);

    TArray<D3D_SHADER_MACRO> D3DDefines;
    BuildD3DDefines(InDesc.Defines, D3DDefines);

    ID3DBlob* ErrorBlob    = nullptr;
    UINT      CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    CompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    FShaderIncludeLoader IncludeLoader(std::filesystem::path(AbsolutePath), OutDependencies);
    const HRESULT        Hr = D3DCompileFromFile(
        AbsolutePath.c_str(),
        D3DDefines.data(),
        &IncludeLoader,
        InDesc.EntryPoint.c_str(),
        InTarget,
        CompileFlags,
        0,
        OutShaderBlob,
        &ErrorBlob);

    ReleaseD3DDefines(D3DDefines);

    if (FAILED(Hr))
    {
        if (ErrorBlob)
        {
            MessageBoxA(nullptr, static_cast<const char*>(ErrorBlob->GetBufferPointer()), InErrorTitle ? InErrorTitle : "Shader Compile Error", MB_OK | MB_ICONERROR);
            ErrorBlob->Release();
        }
        return false;
    }

    if (ErrorBlob)
    {
        ErrorBlob->Release();
    }

    return true;
}

/*
    컴파일된 shader blob에서 material parameter cbuffer 레이아웃을 추출합니다.
    material parameter로 쓰는 b2/b3 슬롯만 수집합니다.
*/
void FShaderProgramBase::ExtractCBufferInfo(ID3DBlob* InShaderBlob, TMap<FString, FMaterialParameterInfo*>& OutLayout) const
{
    if (!InShaderBlob)
    {
        return;
    }

    ID3D11ShaderReflection* Reflector = nullptr;
    const HRESULT           Hr        = D3DReflect(InShaderBlob->GetBufferPointer(), InShaderBlob->GetBufferSize(), IID_ID3D11ShaderReflection, reinterpret_cast<void**>(&Reflector));
    if (FAILED(Hr) || Reflector == nullptr)
    {
        return;
    }

    D3D11_SHADER_DESC ShaderDesc;
    Reflector->GetDesc(&ShaderDesc);

    for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i)
    {
        auto*                    CB = Reflector->GetConstantBufferByIndex(i);
        D3D11_SHADER_BUFFER_DESC CBDesc;
        CB->GetDesc(&CBDesc);

        FString                      BufferName = CBDesc.Name;
        D3D11_SHADER_INPUT_BIND_DESC BindDesc;
        Reflector->GetResourceBindingDescByName(CBDesc.Name, &BindDesc);
        UINT SlotIndex = BindDesc.BindPoint;
        if (SlotIndex != 2 && SlotIndex != 3)
        {
            continue;
        }

        for (UINT j = 0; j < CBDesc.Variables; ++j)
        {
            auto*                      Var = CB->GetVariableByIndex(j);
            D3D11_SHADER_VARIABLE_DESC VarDesc;
            Var->GetDesc(&VarDesc);

            FMaterialParameterInfo* Info = new FMaterialParameterInfo();
            Info->BufferName             = BufferName;
            Info->SlotIndex              = SlotIndex;
            Info->Offset                 = VarDesc.StartOffset;
            Info->Size                   = VarDesc.Size;
            Info->BufferSize             = CBDesc.Size;
            OutLayout[VarDesc.Name]      = Info;
        }
    }

    Reflector->Release();
}

/*
    리플렉션으로 만든 material parameter 레이아웃 객체들을 해제합니다.
*/
void FShaderProgramBase::ReleaseParameterLayout()
{
    for (auto& Pair : ShaderParameterLayout)
    {
        delete Pair.second;
    }
    ShaderParameterLayout.clear();
}

/*
    파생 클래스 소멸 전 공통 리소스를 해제합니다.
*/
void FShaderProgramBase::ReleaseBase()
{
    ReleaseParameterLayout();
}
