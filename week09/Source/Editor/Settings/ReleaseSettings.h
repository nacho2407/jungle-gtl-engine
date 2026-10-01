#pragma once
#include "Core/CoreTypes.h"
#include "Platform/Paths.h"

class FReleaseSettings
{
public:
    FString StartupScenePath = FPaths::ContentRelativePath("Scene/Default.Scene");

    void SaveToFile(const FString& Path) const;
    void LoadFromFile(const FString& Path);

    static FString GetDefaultSettingsPath()
    {
        return FPaths::ToUtf8(FPaths::Combine(FPaths::SettingsDir(), L"Release.ini"));
    }
};
