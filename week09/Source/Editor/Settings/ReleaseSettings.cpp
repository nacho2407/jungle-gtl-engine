#include "ReleaseSettings.h"

#include "SimpleJSON/json.hpp"

#include <filesystem>
#include <fstream>

namespace Key
{
constexpr const char* GameRelease = "GameRelease";
constexpr const char* StartupScenePath = "StartupScenePath";
} // namespace Key

void FReleaseSettings::SaveToFile(const FString& Path) const
{
    using namespace json;

    JSON Root = Object();
    JSON GameReleaseObj = Object();
    GameReleaseObj[Key::StartupScenePath] = StartupScenePath;
    Root[Key::GameRelease] = GameReleaseObj;

    std::filesystem::path FilePath(FPaths::ToWide(Path));
    if (FilePath.has_parent_path())
    {
        std::filesystem::create_directories(FilePath.parent_path());
    }

    std::ofstream File(FilePath);
    if (File.is_open())
    {
        File << Root;
    }
}

void FReleaseSettings::LoadFromFile(const FString& Path)
{
    using namespace json;

    std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
    if (!File.is_open())
    {
        return;
    }

    FString Content((std::istreambuf_iterator<char>(File)),
                    std::istreambuf_iterator<char>());

    JSON Root = JSON::Load(Content);
    if (!Root.hasKey(Key::GameRelease))
    {
        return;
    }

    JSON GameReleaseObj = Root[Key::GameRelease];
    if (GameReleaseObj.hasKey(Key::StartupScenePath))
    {
        StartupScenePath = GameReleaseObj[Key::StartupScenePath].ToString();
    }
}
