#pragma once

#include "Core/CoreTypes.h"
#include "Platform/Paths.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace ShaderDependencyUtils
{
inline uint64 HashString64(const std::string& Value)
{
    uint64 Hash = 1469598103934665603ull;
    for (unsigned char Ch : Value)
    {
        Hash ^= static_cast<uint64>(Ch);
        Hash *= 1099511628211ull;
    }
    return Hash;
}

inline uint64 HashString64(const std::wstring& Value)
{
    uint64 Hash = 1469598103934665603ull;
    for (wchar_t Ch : Value)
    {
        Hash ^= static_cast<uint64>(static_cast<uint32>(Ch));
        Hash *= 1099511628211ull;
    }
    return Hash;
}

inline void HashCombine64(uint64& Seed, uint64 Value)
{
    Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2);
}

inline std::string WStringToUtf8(const std::wstring& WStr)
{
    if (WStr.empty())
    {
        return {};
    }

    const int RequiredBytes = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        WStr.c_str(),
        static_cast<int>(WStr.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (RequiredBytes <= 0)
    {
        return {};
    }

    std::string Result;
    Result.resize(static_cast<size_t>(RequiredBytes));

    const int ConvertedBytes = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        WStr.c_str(),
        static_cast<int>(WStr.size()),
        Result.data(),
        RequiredBytes,
        nullptr,
        nullptr);

    if (ConvertedBytes <= 0)
    {
        return {};
    }

    return Result;
}

inline bool TryExtractIncludePath(const std::string& Line, std::string& OutInclude)
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

template <typename TShaderT>
struct TShaderCacheEntry
{
    std::unique_ptr<TShaderT> Shader;
};

inline std::filesystem::path ResolveIncludePath(const std::filesystem::path& IncludingFile, const std::filesystem::path& IncludePath)
{
    std::vector<std::filesystem::path> SearchRoots;
    const std::filesystem::path Parent = IncludingFile.parent_path();
    SearchRoots.push_back(Parent);

    std::filesystem::path Cursor = Parent;
    while (!Cursor.empty())
    {
        SearchRoots.push_back(Cursor);
        SearchRoots.push_back(Cursor / "Shaders");
        if (Cursor.filename() == "Shaders")
        {
            SearchRoots.push_back(Cursor.parent_path());
        }
        if (Cursor == Cursor.root_path())
        {
            break;
        }
        Cursor = Cursor.parent_path();
    }

    for (const std::filesystem::path& Root : SearchRoots)
    {
        std::error_code Ec;
        const std::filesystem::path Candidate = (Root / IncludePath).lexically_normal();
        if (std::filesystem::exists(Candidate, Ec) && !Ec)
        {
            return Candidate;
        }
    }

    return (Parent / IncludePath).lexically_normal();
}

struct FShaderFileDependency
{
    FString FullPath;
    std::filesystem::file_time_type LastWriteTime{};
    bool bExists = false;
    uint64 DependencyHash = 0;
    std::chrono::steady_clock::time_point LastValidationTime{};
};

inline uint64 BuildDependencyHashRecursive(const std::filesystem::path& FilePath, std::unordered_set<std::wstring>& Visited)
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

    uint64 Hash = HashString64(CanonicalKey);
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

        std::filesystem::path IncludedFile = ResolveIncludePath(Canonical, std::filesystem::path(IncludePath));
        HashCombine64(Hash, BuildDependencyHashRecursive(IncludedFile, Visited));
    }

    return Hash;
}

inline uint64 BuildDependencyHash(const std::filesystem::path& FilePath)
{
    std::unordered_set<std::wstring> Visited;
    return BuildDependencyHashRecursive(FilePath, Visited);
}

inline FShaderFileDependency BuildFileDependency(const FString& FilePath)
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

inline bool HasDependencyChanged(FShaderFileDependency& MutableDependency)
{
    if (MutableDependency.FullPath.empty())
    {
        return false;
    }

    const auto Now = std::chrono::steady_clock::now();
    constexpr auto ValidationInterval = std::chrono::milliseconds(250);
    if (MutableDependency.LastValidationTime.time_since_epoch().count() != 0 &&
        (Now - MutableDependency.LastValidationTime) < ValidationInterval)
    {
        return false;
    }
    MutableDependency.LastValidationTime = Now;

    const FShaderFileDependency Current = BuildFileDependency(MutableDependency.FullPath);
    if (Current.bExists != MutableDependency.bExists)
    {
        return true;
    }
    if (!Current.bExists)
    {
        return false;
    }
    return Current.LastWriteTime != MutableDependency.LastWriteTime || Current.DependencyHash != MutableDependency.DependencyHash;
}
} // namespace ShaderDependencyUtils