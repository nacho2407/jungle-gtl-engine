#include "Path.h"

#include <cassert>

namespace fs = std::filesystem;

FPathConfig FPaths::Config;
bool FPaths::bInitialized = false;

void FPaths::Initialize(const FPathConfig& InConfig)
{
    ValidateConfig(InConfig);
    if (InConfig.EngineRoot.empty() || InConfig.AppRoot.empty())
    {
        return;
    }

    Config.EngineRoot = Normalize(InConfig.EngineRoot);
    Config.AppRoot = Normalize(InConfig.AppRoot);

    Config.EngineContentDir = InConfig.EngineContentDir.empty()
        ? Combine(Config.EngineRoot, L"Resources")
        : Normalize(InConfig.EngineContentDir);

    Config.AppContentDir = InConfig.AppContentDir.empty()
        ? Combine(Config.AppRoot, L"Content")
        : Normalize(InConfig.AppContentDir);

    Config.SavedDir = InConfig.SavedDir.empty()
        ? Combine(Config.AppRoot, L"Saved")
        : Normalize(InConfig.SavedDir);

    Config.ShaderCacheDir = InConfig.ShaderCacheDir.empty()
        ? Combine(Config.SavedDir, L"ShaderCache")
        : Normalize(InConfig.ShaderCacheDir);

    bInitialized = true;
}

bool FPaths::IsInitialized()
{
    return bInitialized;
}

const fs::path& FPaths::EngineRoot()
{
    return GetConfig().EngineRoot;
}

const fs::path& FPaths::AppRoot()
{
    return GetConfig().AppRoot;
}

const fs::path& FPaths::EngineContentDir()
{
    return GetConfig().EngineContentDir;
}

const fs::path& FPaths::AppContentDir()
{
    return GetConfig().AppContentDir;
}

const fs::path& FPaths::SavedDir()
{
    return GetConfig().SavedDir;
}

const fs::path& FPaths::ShaderCacheDir()
{
    return GetConfig().ShaderCacheDir;
}

void FPaths::EnsureRuntimeDirectories()
{
    const FPathConfig& CurrentConfig = GetConfig();

    std::error_code ErrorCode;
    fs::create_directories(CurrentConfig.SavedDir, ErrorCode);
    ErrorCode.clear();
    fs::create_directories(CurrentConfig.ShaderCacheDir, ErrorCode);
}

fs::path FPaths::Combine(const fs::path& Base, const fs::path& Relative)
{
    return Normalize(Base / Relative);
}

const FPathConfig& FPaths::GetConfig()
{
    assert(bInitialized && "FPaths must be initialized by the host application before use.");
    return Config;
}

void FPaths::ValidateConfig(const FPathConfig& InConfig)
{
    assert(!InConfig.EngineRoot.empty() && "FPaths::Initialize requires EngineRoot.");
    assert(!InConfig.AppRoot.empty() && "FPaths::Initialize requires AppRoot.");
}

fs::path FPaths::Normalize(const fs::path& InPath)
{
    std::error_code ErrorCode;
    const fs::path CanonicalPath = fs::weakly_canonical(InPath, ErrorCode);
    if (!ErrorCode)
    {
        return CanonicalPath.lexically_normal();
    }

    return InPath.lexically_normal();
}
