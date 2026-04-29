// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/Shaders/ShaderManager.h"

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"

namespace
{
const char* GetShaderTypeName(EShaderType InType)
{
    switch (InType)
    {
    case EShaderType::Default:
        return "Default";
    case EShaderType::Primitive:
        return "Primitive";
    case EShaderType::Gizmo:
        return "Gizmo";
    case EShaderType::Editor:
        return "Editor";
    case EShaderType::StaticMesh:
        return "StaticMesh";
    case EShaderType::Decal:
        return "Decal";
    case EShaderType::OutlinePostProcess:
        return "OutlinePostProcess";
    case EShaderType::Font:
        return "Font";
    case EShaderType::OverlayFont:
        return "OverlayFont";
    case EShaderType::SubUV:
        return "SubUV";
    case EShaderType::Billboard:
        return "Billboard";
    case EShaderType::HeightFog:
        return "HeightFog";
    case EShaderType::DepthOnly:
        return "DepthOnly";
    case EShaderType::ShadowEncodeVSM:
        return "ShadowEncodeVSM";
    case EShaderType::ShadowEncodeESM:
        return "ShadowEncodeESM";
    case EShaderType::SceneDepth:
        return "SceneDepth";
    case EShaderType::NormalView:
        return "NormalView";
    case EShaderType::FXAA:
        return "FXAA";
    case EShaderType::LightHitMap:
        return "LightHitMap";
    case EShaderType::MAX:
    default:
        return "Unknown";
    }
}

/*
    외부에서 넘겨받은 단일 HLSL 파일을 기본 VS/PS 엔트리 프로그램 desc로 감쌉니다.
    커스텀 셰이더는 레지스트리를 거치지 않으므로 최소한의 그래픽 프로그램 규칙만 적용합니다.
*/
FGraphicsProgramDesc MakeCustomGraphicsDesc(const FString& InPath)
{
    FGraphicsProgramDesc Desc;
    Desc.DebugName = InPath;
    Desc.VS        = { InPath, "VS", {} };
    Desc.PS        = FShaderStageDesc{ InPath, "PS", {} };
    return Desc;
}
} // namespace

// ========== 생명 주기 ==========

/*
    D3D 디바이스와 셰이더 레지스트리를 초기화한 뒤 내장 셰이더를 컴파일합니다.
*/
void FShaderManager::Initialize(ID3D11Device* InDevice)
{
    Device = InDevice;
    MissingBuiltInShaderWarnings.fill(false);
    if (!bIsInitialized)
    {
        bIsInitialized = true;
        UE_LOG(Render, Info, "Shader manager initialized.");
    }

    ShaderRegistry.Initialize();
}

/*
    내장 프로그램과 커스텀 프로그램의 D3D 리소스를 모두 해제하고 캐시를 비웁니다.
*/
void FShaderManager::Release()
{
    UE_LOG(Render, Verbose, "Releasing shader manager resources. BuiltIn=%u Custom=%u",
        static_cast<uint32>(EShaderType::MAX),
        static_cast<uint32>(CustomShaderCache.size()));
    for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
    {
        Shaders[i].Release();
        BuiltInShaderFiles[i] = {};
    }

    for (auto& [Key, Entry] : CustomShaderCache)
    {
        if (Entry.Shader)
        {
            Entry.Shader->Release();
        }
    }
    CustomShaderCache.clear();
    MissingBuiltInShaderWarnings.fill(false);

    Device         = nullptr;
    bIsInitialized = false;
}

// ========== 핫 리로드 ==========

/*
    디버그 빌드에서 등록된 셰이더 파일 변경을 확인하고 필요한 프로그램만 다시 컴파일합니다.
*/
void FShaderManager::TickHotReload()
{
#if defined(_DEBUG)
    if (!Device)
    {
        return;
    }

    for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
    {
        const EShaderType ShaderType = static_cast<EShaderType>(i);
        if (ShaderRegistry.Find(ShaderType) == nullptr)
        {
            continue;
        }

        RefreshBuiltInShader(ShaderType);
    }

    for (auto& Pair : CustomShaderCache)
    {
        RefreshCustomShader(Pair.second, Pair.first);
    }
#endif
}

// ========== 조회 ==========

/*
    셰이더 타입에 대응하는 내장 그래픽 프로그램을 반환합니다.
    핫 리로드 검사는 프레임 시작의 TickHotReload에서 일괄 처리합니다.
 */
FGraphicsProgram* FShaderManager::GetShader(EShaderType InType)
{
    const uint32 Idx = (uint32)InType;
    if (Idx >= (uint32)EShaderType::MAX)
    {
        return nullptr;
    }
    if (Device)
    {
        if (!Shaders[Idx].IsValid())
        {
            RefreshBuiltInShader(InType);
        }
    }
    return &Shaders[Idx];
}

/*
    정규화된 키로 캐시된 커스텀 셰이더 프로그램을 찾습니다.
*/
FGraphicsProgram* FShaderManager::GetCustomShader(const FString& Key)
{
    const FString NormalizedKey = FPaths::FromPath(FPaths::ToPath(FPaths::ToWide(Key)).lexically_normal());
    auto          It            = CustomShaderCache.find(NormalizedKey);
    if (It == CustomShaderCache.end())
    {
        return nullptr;
    }

#if defined(_DEBUG)
    RefreshCustomShader(It->second, NormalizedKey);
#endif
    return It->second.Shader.get();
}

// ========== 커스텀 프로그램 컴파일 ==========

/*
    파일 경로 하나로 구성된 커스텀 VS/PS 프로그램을 생성하거나 기존 캐시 항목을 갱신합니다.
*/
FGraphicsProgram* FShaderManager::CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath)
{
    Device = InDevice ? InDevice : Device;
    if (!Device || InFilePath == nullptr)
    {
        UE_LOG(Render, Warning, "CreateCustomShader skipped because device or file path was invalid.");
        return nullptr;
    }

    const FString Key           = ShaderDependencyUtils::WStringToUtf8(std::wstring(InFilePath));
    const FString NormalizedKey = FPaths::FromPath(FPaths::ToPath(FPaths::ToWide(Key)).lexically_normal());

    auto It = CustomShaderCache.find(NormalizedKey);
    if (It != CustomShaderCache.end())
    {
        UE_LOG(Render, Verbose, "Reusing custom shader cache entry: %s", NormalizedKey.c_str());
        RefreshCustomShader(It->second, NormalizedKey);
        return It->second.Shader.get();
    }

    FCustomShaderCacheEntry Entry;
    Entry.Shader                    = std::make_unique<FGraphicsProgram>();
    const FGraphicsProgramDesc Desc = MakeCustomGraphicsDesc(NormalizedKey);
    if (!Entry.Shader->Create(Device, Desc))
    {
        UE_LOG(Render, Error, "Failed to create custom shader: %s", NormalizedKey.c_str());
        return nullptr;
    }

    Entry.SourceFile                 = ShaderDependencyUtils::BuildFileDependency(NormalizedKey);
    auto* RawPtr                     = Entry.Shader.get();
    CustomShaderCache[NormalizedKey] = std::move(Entry);
    UE_LOG(Render, Info, "Created custom shader program: %s", NormalizedKey.c_str());
    return RawPtr;
}

// ========== 내장 프로그램 컴파일 ==========

/*
    레지스트리에 등록된 desc를 기준으로 내장 셰이더 프로그램을 컴파일합니다.
    의존 파일이 바뀌지 않았으면 기존 프로그램을 그대로 유지합니다.
*/
void FShaderManager::RefreshBuiltInShader(EShaderType InType)
{
    const uint32 Idx = (uint32)InType;
    if (Idx >= (uint32)EShaderType::MAX || !Device)
    {
        return;
    }

    const FGraphicsProgramDesc* Desc = ShaderRegistry.Find(InType);
    if (!Desc)
    {
        if (!MissingBuiltInShaderWarnings[Idx])
        {
            MissingBuiltInShaderWarnings[Idx] = true;
            UE_LOG(Render, Warning, "Built-in shader descriptor was not found for type %s.", GetShaderTypeName(InType));
        }
        return;
    }

    MissingBuiltInShaderWarnings[Idx] = false;

    auto& Dependency = BuiltInShaderFiles[Idx];
    if (Dependency.bExists && !ShaderDependencyUtils::HasDependencyChanged(Dependency))
    {
        return;
    }

    if (Shaders[Idx].Create(Device, *Desc))
    {
        Dependency = ShaderDependencyUtils::BuildFileDependency(Desc->VS.FilePath);
        UE_LOG(Render, Verbose, "Compiled built-in shader [%s] from %s", GetShaderTypeName(InType), Desc->VS.FilePath.c_str());
    }
    else
    {
        UE_LOG(Render, Error, "Failed to compile built-in shader [%s] from %s", GetShaderTypeName(InType), Desc->VS.FilePath.c_str());
    }
}

// ========== 커스텀 프로그램 리로드 ==========

/*
    커스텀 셰이더 파일의 변경 여부를 검사하고 바뀐 경우 다시 컴파일합니다.
*/
bool FShaderManager::RefreshCustomShader(FCustomShaderCacheEntry& Entry, const FString& NormalizedKey)
{
    if (!Device || !Entry.Shader)
    {
        return false;
    }

    if (Entry.SourceFile.bExists && !ShaderDependencyUtils::HasDependencyChanged(Entry.SourceFile))
    {
        return true;
    }

    const FGraphicsProgramDesc Desc = MakeCustomGraphicsDesc(NormalizedKey);
    if (Entry.Shader->Create(Device, Desc))
    {
        Entry.SourceFile = ShaderDependencyUtils::BuildFileDependency(NormalizedKey);
        UE_LOG(Render, Verbose, "Refreshed custom shader program: %s", NormalizedKey.c_str());
        return true;
    }

    UE_LOG(Render, Error, "Failed to refresh custom shader program: %s", NormalizedKey.c_str());
    return false;
}
