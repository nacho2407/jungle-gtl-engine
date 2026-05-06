#include "Editor/Settings/ProjectSettings.h"

#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace ProjectKey
{
	constexpr const char* Build = "Build";
	constexpr const char* GameName = "GameName";
	constexpr const char* StartupScene = "StartupScene";
	constexpr const char* IncludedScenes = "IncludedScenes";
	constexpr const char* PlayerControllerClass = "PlayerControllerClass";
	constexpr const char* OutputDirectory = "OutputDirectory";
	constexpr const char* IconPath = "IconPath";
	constexpr const char* SplashImagePath = "SplashImagePath";
	constexpr const char* SplashMinSeconds = "SplashMinSeconds";
	constexpr const char* Configuration = "Configuration";
	constexpr const char* bCleanOutput = "bCleanOutput";
	constexpr const char* bRunAfterBuild = "bRunAfterBuild";
	constexpr const char* Editor = "Editor";
	constexpr const char* LastScenePath = "LastScenePath";
	constexpr const char* NewScene = "New Scene";
}

static FString NormalizeProjectPathForStorage(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty() || NormalizedPath == ProjectKey::NewScene)
	{
		return ProjectKey::NewScene;
	}

	const std::filesystem::path FsPath(FPaths::ToWide(NormalizedPath));
	if (FsPath.is_absolute())
	{
		const std::filesystem::path RootPath(FPaths::RootDir());
		std::error_code Ec;
		const std::filesystem::path RelativePath = std::filesystem::relative(FsPath, RootPath, Ec);
		if (!Ec && !RelativePath.empty())
		{
			const std::wstring RelativeText = RelativePath.generic_wstring();
			if (RelativeText.rfind(L"..", 0) != 0)
			{
				return FPaths::Normalize(FPaths::ToUtf8(RelativeText));
			}
		}
	}
	return NormalizedPath;
}

void FProjectSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();
	JSON Build = Object();
	Build[ProjectKey::GameName] = BuildSettings.GameName;
	Build[ProjectKey::StartupScene] = BuildSettings.StartupScene;
	Build[ProjectKey::PlayerControllerClass] = BuildSettings.PlayerControllerClass;
	Build[ProjectKey::OutputDirectory] = BuildSettings.OutputDirectory;
	Build[ProjectKey::IconPath] = BuildSettings.IconPath;
	Build[ProjectKey::SplashImagePath] = BuildSettings.SplashImagePath;
	Build[ProjectKey::SplashMinSeconds] = BuildSettings.SplashMinSeconds;
	Build[ProjectKey::Configuration] = static_cast<int32>(BuildSettings.Configuration);
	Build[ProjectKey::bCleanOutput] = BuildSettings.bCleanOutput;
	Build[ProjectKey::bRunAfterBuild] = BuildSettings.bRunAfterBuild;

	JSON Scenes = Array();
	for (const FString& Scene : BuildSettings.IncludedScenes)
	{
		Scenes.append(Scene);
	}
	Build[ProjectKey::IncludedScenes] = Scenes;
	Root[ProjectKey::Build] = Build;

	JSON Editor = Object();
	Editor[ProjectKey::LastScenePath] = NormalizeProjectPathForStorage(LastScenePath);
	Root[ProjectKey::Editor] = Editor;

	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
	{
		std::filesystem::create_directories(FilePath.parent_path());
	}

	std::ofstream File(FilePath, std::ios::trunc);
	if (File.is_open())
	{
		File << Root;
	}
}

void FProjectSettings::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		LastScenePath = ProjectKey::NewScene;
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(Content);

	if (Root.hasKey(ProjectKey::Build))
	{
		JSON Build = Root[ProjectKey::Build];
		if (Build.hasKey(ProjectKey::GameName))
			BuildSettings.GameName = Build[ProjectKey::GameName].ToString();
		if (Build.hasKey(ProjectKey::StartupScene))
			BuildSettings.StartupScene = FPaths::Normalize(Build[ProjectKey::StartupScene].ToString());
		if (Build.hasKey(ProjectKey::PlayerControllerClass))
			BuildSettings.PlayerControllerClass = Build[ProjectKey::PlayerControllerClass].ToString();
		if (Build.hasKey(ProjectKey::OutputDirectory))
			BuildSettings.OutputDirectory = FPaths::Normalize(Build[ProjectKey::OutputDirectory].ToString());
		if (Build.hasKey(ProjectKey::IconPath))
			BuildSettings.IconPath = FPaths::Normalize(Build[ProjectKey::IconPath].ToString());
		if (Build.hasKey(ProjectKey::SplashImagePath))
			BuildSettings.SplashImagePath = FPaths::Normalize(Build[ProjectKey::SplashImagePath].ToString());
		if (Build.hasKey(ProjectKey::SplashMinSeconds))
			BuildSettings.SplashMinSeconds = std::clamp(static_cast<float>(Build[ProjectKey::SplashMinSeconds].ToFloat()), 3.0f, 10.0f);
		if (Build.hasKey(ProjectKey::Configuration))
			BuildSettings.Configuration = Build[ProjectKey::Configuration].ToInt() == static_cast<int32>(EGameBuildConfiguration::Shipping)
				? EGameBuildConfiguration::Shipping
				: EGameBuildConfiguration::Development;
		if (Build.hasKey(ProjectKey::bCleanOutput))
			BuildSettings.bCleanOutput = Build[ProjectKey::bCleanOutput].ToBool();
		if (Build.hasKey(ProjectKey::bRunAfterBuild))
			BuildSettings.bRunAfterBuild = Build[ProjectKey::bRunAfterBuild].ToBool();

		BuildSettings.IncludedScenes.clear();
		if (Build.hasKey(ProjectKey::IncludedScenes))
		{
			for (auto& Scene : Build[ProjectKey::IncludedScenes].ArrayRange())
			{
				const FString NormalizedScene = FPaths::Normalize(Scene.ToString());
				if (!NormalizedScene.empty())
				{
					BuildSettings.IncludedScenes.push_back(NormalizedScene);
				}
			}
		}
	}

	if (Root.hasKey(ProjectKey::Editor) && Root[ProjectKey::Editor].hasKey(ProjectKey::LastScenePath))
	{
		LastScenePath = FPaths::Normalize(Root[ProjectKey::Editor][ProjectKey::LastScenePath].ToString());
	}
	if (LastScenePath.empty())
	{
		LastScenePath = ProjectKey::NewScene;
	}
}

void FProjectSettings::SetLastScenePath(const FString& ScenePath)
{
	LastScenePath = NormalizeProjectPathForStorage(ScenePath);
}

bool FProjectSettings::HasSavedLastScenePath() const
{
	return !LastScenePath.empty() && LastScenePath != ProjectKey::NewScene;
}
