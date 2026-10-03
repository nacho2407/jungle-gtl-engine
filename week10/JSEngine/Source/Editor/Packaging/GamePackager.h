#pragma once

#include "Editor/Packaging/GameBuildSettings.h"

struct FGamePackageResult
{
    bool bSucceeded = false;
    FString Message;
    FString OutputDirectory;
};

class FGamePackager
{
public:
    static FGamePackageResult BuildAndPackage(const FGameBuildSettings& Settings);
    static FString ResolveOutputDirectoryForDisplay(const FString& OutputDirectory);

private:
    static bool PrepareBrandingResources(const FGameBuildSettings& Settings, FString& OutMessage);
    static bool RunMSBuild(const FGameBuildSettings& Settings, FString& OutMessage);
    static bool CopyPackageFiles(const FGameBuildSettings& Settings, FString& OutMessage);
};
