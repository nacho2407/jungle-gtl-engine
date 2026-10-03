#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Paths.h"
#include "Core/Singleton.h"
#include "Editor/Packaging/GameBuildSettings.h"

class FProjectSettings : public TSingleton<FProjectSettings>
{
	friend class TSingleton<FProjectSettings>;

public:
	FGameBuildSettings BuildSettings;
	FString LastScenePath = "New Scene";

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	void SetLastScenePath(const FString& ScenePath);
	bool HasSavedLastScenePath() const;

	static FString GetDefaultSettingsPath()
	{
		return FPaths::ToUtf8(FPaths::SettingsDir() + L"Project.ini");
	}

private:
	FProjectSettings() = default;
};
