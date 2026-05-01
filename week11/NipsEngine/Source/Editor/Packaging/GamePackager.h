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

private:
    static bool RunMSBuild(const FGameBuildSettings& Settings, FString& OutMessage);
    static bool CopyPackageFiles(const FGameBuildSettings& Settings, FString& OutMessage);
};
