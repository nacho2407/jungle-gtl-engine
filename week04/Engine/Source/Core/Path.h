#pragma once

#include "EngineAPI.h"

#include <filesystem>

struct FPathConfig
{
    std::filesystem::path EngineRoot;
    std::filesystem::path AppRoot;

    std::filesystem::path EngineContentDir;
    std::filesystem::path AppContentDir;
    std::filesystem::path SavedDir;
    std::filesystem::path ShaderCacheDir;
};

class ENGINE_API FPaths
{
public:
    static void Initialize(const FPathConfig& InConfig);
    static bool IsInitialized();

    static const std::filesystem::path& EngineRoot();
    static const std::filesystem::path& AppRoot();
    static const std::filesystem::path& EngineContentDir();
    static const std::filesystem::path& AppContentDir();
    static const std::filesystem::path& SavedDir();
    static const std::filesystem::path& ShaderCacheDir();

    static void EnsureRuntimeDirectories();

    static std::filesystem::path Combine(const std::filesystem::path& Base, const std::filesystem::path& Relative);

private:
    static const FPathConfig& GetConfig();
    static void ValidateConfig(const FPathConfig& InConfig);
    static std::filesystem::path Normalize(const std::filesystem::path& InPath);

    static FPathConfig Config;
    static bool bInitialized;
};

