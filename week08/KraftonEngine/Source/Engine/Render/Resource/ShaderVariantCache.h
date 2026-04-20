#pragma once

#include "Core/CoreTypes.h"
#include "Platform/Paths.h"
#include "Render/D3D11/Resource/Shader.h"

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <memory>
#include <unordered_set>
#include <fstream>
#include <string>
#include <system_error>

struct FShaderMacroDefine
{
    FString Name;
    FString Value;
};

struct FShaderVariantDesc
{
    FString FilePath;
    FString VSEntry;
    FString PSEntry;
    TArray<FShaderMacroDefine> Defines;

    FString BuildKey() const
    {
        FString Key = FilePath + "|VS=" + VSEntry + "|PS=" + PSEntry;
        for (const FShaderMacroDefine& Def : Defines)
        {
            Key += "|" + Def.Name + "=" + Def.Value;
        }
        return Key;
    }
};

struct FShaderVariantFileDependency
{
    FString FullPath;
    std::filesystem::file_time_type LastWriteTime{};
    bool bExists = false;
    uint64 DependencyHash = 0;
    std::chrono::steady_clock::time_point LastValidationTime{};
};

struct FShaderVariantCacheEntry
{
    std::unique_ptr<FShader> Shader;
    FShaderVariantFileDependency SourceFile;
};

class FShaderVariantCache
{
public:
    void Initialize(ID3D11Device* InDevice)
    {
        Device = InDevice;
    }

    void Release()
    {
        for (auto& Pair : Cache)
        {
            if (Pair.second.Shader)
            {
                Pair.second.Shader->Release();
            }
        }
        Cache.clear();

        for (auto& Shader : RetiredShaders)
        {
            if (Shader)
            {
                Shader->Release();
            }
        }
        RetiredShaders.clear();

        Device = nullptr;
    }

    FShader* GetOrCreate(const FShaderVariantDesc& Desc)
    {
        if (!Device)
        {
            return nullptr;
        }

        const FString Key = Desc.BuildKey();
        auto It = Cache.find(Key);
        if (It != Cache.end())
        {
            if (!HasDependencyChanged(It->second.SourceFile))
            {
                return It->second.Shader.get();
            }

            if (It->second.Shader)
            {
                RetiredShaders.push_back(std::move(It->second.Shader));
            }
            Cache.erase(It);
        }

        auto NewShader = std::make_unique<FShader>();
        TArray<D3D_SHADER_MACRO> D3DDefines;
        BuildD3DDefines(Desc.Defines, D3DDefines);

        const std::wstring WidePath = FPaths::ToWide(Desc.FilePath);
        NewShader->Create(Device, WidePath.c_str(), Desc.VSEntry.c_str(), Desc.PSEntry.c_str(),
                          D3DDefines.empty() ? nullptr : D3DDefines.data());

        for (D3D_SHADER_MACRO& Macro : D3DDefines)
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

        FShader* RawShader = NewShader.get();
        FShaderVariantCacheEntry Entry;
        Entry.SourceFile = BuildFileDependency(Desc.FilePath);
        Entry.Shader = std::move(NewShader);
        Cache.emplace(Key, std::move(Entry));
        return RawShader;
    }


private:
    uint64 HashString64(const std::string& Value) const
    {
        uint64 Hash = 1469598103934665603ull;
        for (unsigned char Ch : Value)
        {
            Hash ^= static_cast<uint64>(Ch);
            Hash *= 1099511628211ull;
        }
        return Hash;
    }

    void HashCombine64(uint64& Seed, uint64 Value) const
    {
        Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2);
    }

    bool TryExtractIncludePath(const std::string& Line, std::string& OutInclude) const
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

    uint64 BuildDependencyHashRecursive(const std::filesystem::path& FilePath, std::unordered_set<std::wstring>& Visited) const
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

        uint64 Hash = HashString64(std::string(CanonicalKey.begin(), CanonicalKey.end()));
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

    uint64 BuildDependencyHash(const std::filesystem::path& FilePath) const
    {
        std::unordered_set<std::wstring> Visited;
        return BuildDependencyHashRecursive(FilePath, Visited);
    }

    void BuildD3DDefines(const TArray<FShaderMacroDefine>& InDefines, TArray<D3D_SHADER_MACRO>& OutDefines) const
    {
        OutDefines.clear();
        OutDefines.reserve(InDefines.size() + 1);

        for (const FShaderMacroDefine& Def : InDefines)
        {
            D3D_SHADER_MACRO Macro = {};
            Macro.Name = _strdup(Def.Name.c_str());
            Macro.Definition = _strdup(Def.Value.c_str());
            OutDefines.push_back(Macro);
        }

        OutDefines.push_back({ nullptr, nullptr });
    }

    FShaderVariantFileDependency BuildFileDependency(const FString& FilePath) const
    {
        FShaderVariantFileDependency Dependency;

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

    bool HasDependencyChanged(const FShaderVariantFileDependency& Dependency) const
    {
        if (Dependency.FullPath.empty())
        {
            return false;
        }

        auto& MutableDependency = const_cast<FShaderVariantFileDependency&>(Dependency);
        const auto Now = std::chrono::steady_clock::now();
        constexpr auto ValidationInterval = std::chrono::milliseconds(250);
        if (MutableDependency.LastValidationTime.time_since_epoch().count() != 0 &&
            (Now - MutableDependency.LastValidationTime) < ValidationInterval)
        {
            return false;
        }
        MutableDependency.LastValidationTime = Now;

        const FShaderVariantFileDependency Current = BuildFileDependency(Dependency.FullPath);
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

private:
    ID3D11Device* Device = nullptr;
    TMap<FString, FShaderVariantCacheEntry> Cache;
    TArray<std::unique_ptr<FShader>> RetiredShaders;
};
