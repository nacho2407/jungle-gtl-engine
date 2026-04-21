#include "Render/Resources/Shaders/ShaderManager.h"

#include "Platform/Paths.h"

#include <filesystem>
#include <system_error>

namespace
{
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
} // namespace

void FShaderManager::Initialize(ID3D11Device* InDevice)
{
    Device = InDevice;
    if (!bIsInitialized)
    {
        bIsInitialized = true;
    }

    for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
    {
        RefreshBuiltInShader(static_cast<EShaderType>(i));
    }
}

void FShaderManager::Release()
{
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

    Device = nullptr;
    bIsInitialized = false;
}

void FShaderManager::TickHotReload()
{
    if (!Device)
    {
        return;
    }

    for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
    {
        RefreshBuiltInShader(static_cast<EShaderType>(i));
    }

    for (auto& Pair : CustomShaderCache)
    {
        RefreshCustomShader(Pair.second, Pair.first);
    }
}

FShader* FShaderManager::GetShader(EShaderType InType)
{
    const uint32 Idx = (uint32)InType;
    if (Idx >= (uint32)EShaderType::MAX)
    {
        return nullptr;
    }
    if (Device)
    {
        RefreshBuiltInShader(InType);
    }
    return &Shaders[Idx];
}

FShader* FShaderManager::GetCustomShader(const FString& Key)
{
    const FString NormalizedKey = FPaths::FromPath(FPaths::ToPath(FPaths::ToWide(Key)).lexically_normal());
    auto It = CustomShaderCache.find(NormalizedKey);
    if (It == CustomShaderCache.end())
    {
        return nullptr;
    }

    RefreshCustomShader(It->second, NormalizedKey);
    return It->second.Shader.get();
}

FShader* FShaderManager::CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath)
{
    Device = InDevice ? InDevice : Device;
    if (!Device || InFilePath == nullptr)
    {
        return nullptr;
    }

    const FString Key = ShaderDependencyUtils::WStringToUtf8(std::wstring(InFilePath));
    const FString NormalizedKey = FPaths::FromPath(FPaths::ToPath(FPaths::ToWide(Key)).lexically_normal());

    auto It = CustomShaderCache.find(NormalizedKey);
    if (It != CustomShaderCache.end())
    {
        RefreshCustomShader(It->second, NormalizedKey);
        return It->second.Shader.get();
    }

    FCustomShaderCacheEntry Entry;
    Entry.Shader = std::make_unique<FShader>();
    const std::wstring AbsoluteShaderPath = MakeAbsoluteShaderPath(NormalizedKey);
    if (!Entry.Shader->Create(Device, AbsoluteShaderPath.c_str(), "VS", "PS"))
    {
        return nullptr;
    }

    Entry.SourceFile = ShaderDependencyUtils::BuildFileDependency(NormalizedKey);
    auto* RawPtr = Entry.Shader.get();
    CustomShaderCache[NormalizedKey] = std::move(Entry);
    return RawPtr;
}

void FShaderManager::RefreshBuiltInShader(EShaderType InType)
{
    const uint32 Idx = (uint32)InType;
    if (Idx >= (uint32)EShaderType::MAX || !Device)
    {
        return;
    }

    const FString ShaderPath = GetBuiltInShaderPath(InType);
    if (ShaderPath.empty())
    {
        return;
    }

    auto& Dependency = BuiltInShaderFiles[Idx];
    if (Dependency.bExists && !ShaderDependencyUtils::HasDependencyChanged(Dependency))
    {
        return;
    }

    const std::wstring AbsoluteShaderPath = MakeAbsoluteShaderPath(ShaderPath);
    if (Shaders[Idx].Create(Device, AbsoluteShaderPath.c_str(), "VS", "PS"))
    {
        Dependency = ShaderDependencyUtils::BuildFileDependency(ShaderPath);
    }
}

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

    const std::wstring AbsoluteShaderPath = MakeAbsoluteShaderPath(NormalizedKey);
    if (Entry.Shader->Create(Device, AbsoluteShaderPath.c_str(), "VS", "PS"))
    {
        Entry.SourceFile = ShaderDependencyUtils::BuildFileDependency(NormalizedKey);
        return true;
    }

    return false;
}

FString FShaderManager::GetBuiltInShaderPath(EShaderType InType) const
{
    switch (InType)
    {
    case EShaderType::Primitive:
        return "Shaders/Editor/Primitive.hlsl";
    case EShaderType::Gizmo:
        return "Shaders/Editor/Gizmo.hlsl";
    case EShaderType::Editor:
        return "Shaders/Editor/Editor.hlsl";
    case EShaderType::StaticMesh:
        return "Shaders/Materials/StaticMeshShader.hlsl";
    case EShaderType::Decal:
        return "Shaders/Materials/DecalShader.hlsl";
    case EShaderType::OutlinePostProcess:
        return "Shaders/Passes/PostProcess/OutlinePostProcess.hlsl";
    case EShaderType::SceneDepth:
        return "Shaders/ViewModes/SceneDepth.hlsl";
    case EShaderType::NormalView:
        return "Shaders/ViewModes/NormalView.hlsl";
    case EShaderType::FXAA:
        return "Shaders/Passes/PostProcess/FXAA.hlsl";
    case EShaderType::Font:
        return "Shaders/Editor/ShaderFont.hlsl";
    case EShaderType::OverlayFont:
        return "Shaders/Editor/ShaderOverlayFont.hlsl";
    case EShaderType::SubUV:
        return "Shaders/Editor/ShaderSubUV.hlsl";
    case EShaderType::Billboard:
        return "Shaders/Editor/ShaderBillboard.hlsl";
    case EShaderType::HeightFog:
        return "Shaders/Passes/Scene/HeightFog.hlsl";
    case EShaderType::DepthOnly:
        return "Shaders/Passes/Scene/DepthOnly.hlsl";
    default:
        return "";
    }
}
