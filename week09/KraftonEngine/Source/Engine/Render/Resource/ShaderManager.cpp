#include "ShaderManager.h"
#include <string>
#include <fstream>
#include <unordered_set>
#include <Windows.h>
#include "Platform/Paths.h"

namespace
{
inline std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty())
        return std::string();

    int sizeNeeded = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.data(),
        (int)wstr.size(),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (sizeNeeded <= 0)
        return std::string();

    std::string result(sizeNeeded, 0);

    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.data(),
        (int)wstr.size(),
        &result[0],
        sizeNeeded,
        nullptr,
        nullptr);

    return result;
}

uint64 HashString64(const std::string& Value)
{
    uint64 Hash = 1469598103934665603ull;
    for (unsigned char Ch : Value)
    {
        Hash ^= static_cast<uint64>(Ch);
        Hash *= 1099511628211ull;
    }
    return Hash;
}

void HashCombine64(uint64& Seed, uint64 Value)
{
    Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2);
}

bool TryExtractIncludePath(const std::string& Line, std::string& OutInclude)
{
    const size_t IncludePos = Line.find("#include");
    if (IncludePos == std::string::npos)
    {
        return false;
    }

    const size_t OpenQuote = Line.find('"', IncludePos);
    if (OpenQuote == std::string::npos)
    {
        return false;
    }

    const size_t CloseQuote = Line.find('"', OpenQuote + 1);
    if (CloseQuote == std::string::npos || CloseQuote <= OpenQuote + 1)
    {
        return false;
    }

    OutInclude = Line.substr(OpenQuote + 1, CloseQuote - OpenQuote - 1);
    return true;
}

uint64 BuildDependencyHashRecursive(const std::filesystem::path& FilePath, std::unordered_set<std::wstring>& Visited)
{
    std::error_code Ec;
    std::filesystem::path Canonical = std::filesystem::weakly_canonical(FilePath, Ec);
    if (Ec)
    {
        Canonical = FilePath.lexically_normal();
    }

    const std::wstring CanonicalKey = Canonical.generic_wstring();
    if (!Visited.insert(CanonicalKey).second)
    {
        return 0;
    }

    uint64 Hash = HashString64(WStringToString(CanonicalKey));
    const bool bExists = std::filesystem::exists(Canonical, Ec) && !Ec;
    HashCombine64(Hash, bExists ? 1ull : 0ull);
    if (!bExists)
    {
        return Hash;
    }

    const auto LastWrite = std::filesystem::last_write_time(Canonical, Ec);
    if (!Ec)
    {
        HashCombine64(Hash, static_cast<uint64>(LastWrite.time_since_epoch().count()));
    }

    std::ifstream File(Canonical);
    if (!File.is_open())
    {
        return Hash;
    }

    std::string Line;
    while (std::getline(File, Line))
    {
        std::string IncludePath;
        if (!TryExtractIncludePath(Line, IncludePath))
        {
            continue;
        }

        std::filesystem::path IncludedFile = (Canonical.parent_path() / std::filesystem::path(IncludePath)).lexically_normal();
        HashCombine64(Hash, BuildDependencyHashRecursive(IncludedFile, Visited));
    }

    return Hash;
}

uint64 BuildDependencyHash(const std::filesystem::path& FilePath)
{
    std::unordered_set<std::wstring> Visited;
    return BuildDependencyHashRecursive(FilePath, Visited);
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

    for (auto& Shader : RetiredCustomShaders)
    {
        if (Shader)
        {
            Shader->Release();
        }
    }
    RetiredCustomShaders.clear();

    Device = nullptr;
    bIsInitialized = false;
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
    if (It != CustomShaderCache.end())
    {
        if (!HasDependencyChanged(It->second.SourceFile))
        {
            return It->second.Shader.get();
        }
    }
    return nullptr;
}

FShader* FShaderManager::CreateCustomShader(ID3D11Device* InDevice, const wchar_t* InFilePath)
{
    Device = InDevice ? InDevice : Device;
    const FString Key = WStringToString(std::wstring(InFilePath));
    const FString NormalizedKey = FPaths::FromPath(FPaths::ToPath(FPaths::ToWide(Key)).lexically_normal());

    auto It = CustomShaderCache.find(NormalizedKey);
    if (It != CustomShaderCache.end())
    {
        if (!HasDependencyChanged(It->second.SourceFile))
        {
            return It->second.Shader.get();
        }

        if (It->second.Shader)
        {
            RetiredCustomShaders.push_back(std::move(It->second.Shader));
        }
        CustomShaderCache.erase(It);
    }

    auto NewShader = std::make_unique<FShader>();
    NewShader->Create(Device, FPaths::ToWide(NormalizedKey).c_str(), "VS", "PS");
    auto* RawPtr = NewShader.get();

    FCustomShaderCacheEntry Entry;
    Entry.SourceFile = BuildFileDependency(NormalizedKey);
    Entry.Shader = std::move(NewShader);
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

    const FShaderFileDependency CurrentFile = BuildFileDependency(ShaderPath);
    if (BuiltInShaderFiles[Idx].bExists && !HasDependencyChanged(BuiltInShaderFiles[Idx]))
    {
        return;
    }

    Shaders[Idx].Create(Device, FPaths::ToWide(ShaderPath).c_str(), "VS", "PS");
    BuiltInShaderFiles[Idx] = CurrentFile;
}

FString FShaderManager::GetBuiltInShaderPath(EShaderType InType) const
{
    switch (InType)
    {
    case EShaderType::Primitive: return "Shaders/Primitive.hlsl";
    case EShaderType::Gizmo: return "Shaders/Gizmo.hlsl";
    case EShaderType::Editor: return "Shaders/Editor.hlsl";
    case EShaderType::StaticMesh: return "Shaders/StaticMeshShader.hlsl";
    case EShaderType::Decal: return "Shaders/DecalShader.hlsl";
    case EShaderType::OutlinePostProcess: return "Shaders/OutlinePostProcess.hlsl";
    case EShaderType::SceneDepth: return "Shaders/SceneDepth.hlsl";
    case EShaderType::NormalView: return "Shaders/NormalView.hlsl";
    case EShaderType::FXAA: return "Shaders/FXAA.hlsl";
    case EShaderType::Font: return "Shaders/ShaderFont.hlsl";
    case EShaderType::OverlayFont: return "Shaders/ShaderOverlayFont.hlsl";
    case EShaderType::SubUV: return "Shaders/ShaderSubUV.hlsl";
    case EShaderType::Billboard: return "Shaders/ShaderBillboard.hlsl";
    case EShaderType::HeightFog: return "Shaders/HeightFog.hlsl";
    case EShaderType::DepthOnly: return "Shaders/DepthOnly.hlsl";
    default: return "";
    }
}

FShaderFileDependency FShaderManager::BuildFileDependency(const FString& FilePath) const
{
    FShaderFileDependency Dependency;

    std::filesystem::path Path = FPaths::ToPath(FPaths::ToWide(FilePath));
    if (!Path.is_absolute())
    {
        Path = FPaths::ToPath(FPaths::RootDir()) / Path;
    }

    std::error_code Ec;
    std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, Ec);
    if (Ec)
    {
        Canonical = Path.lexically_normal();
    }

    Dependency.FullPath = FPaths::FromPath(Canonical);
    Dependency.bExists = std::filesystem::exists(Canonical, Ec) && !Ec;
    if (Dependency.bExists)
    {
        Dependency.LastWriteTime = std::filesystem::last_write_time(Canonical, Ec);
        if (Ec)
        {
            Dependency.bExists = false;
            Dependency.LastWriteTime = {};
        }
    }

    Dependency.DependencyHash = BuildDependencyHash(Canonical);
    return Dependency;
}

bool FShaderManager::HasDependencyChanged(const FShaderFileDependency& Dependency) const
{
    if (Dependency.FullPath.empty())
    {
        return false;
    }

    auto& MutableDependency = const_cast<FShaderFileDependency&>(Dependency);
    const auto Now = std::chrono::steady_clock::now();
    constexpr auto ValidationInterval = std::chrono::milliseconds(250);
    if (MutableDependency.LastValidationTime.time_since_epoch().count() != 0 &&
        (Now - MutableDependency.LastValidationTime) < ValidationInterval)
    {
        return false;
    }
    MutableDependency.LastValidationTime = Now;

    const FShaderFileDependency Current = BuildFileDependency(Dependency.FullPath);
    if (Current.bExists != Dependency.bExists)
    {
        return true;
    }

    if (!Current.bExists)
    {
        return false;
    }

    return Current.LastWriteTime != Dependency.LastWriteTime || Current.DependencyHash != Dependency.DependencyHash;
}
