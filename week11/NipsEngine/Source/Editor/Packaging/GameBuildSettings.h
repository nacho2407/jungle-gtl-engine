#pragma once

#include "Core/CoreMinimal.h"

enum class EGameBuildConfiguration : uint8
{
    Development,
    Shipping
};

struct FGameBuildSettings
{
    FString GameName = "NipsGame";
    FString StartupScene = "Asset/Scene/Main.Scene";
    TArray<FString> IncludedScenes;
    FString PlayerControllerClass = "APlayerController";
    FString OutputDirectory = "Builds/Windows/NipsGame";
    FString IconPath;
    FString SplashImagePath;
    float SplashMinSeconds = 3.0f;
    EGameBuildConfiguration Configuration = EGameBuildConfiguration::Development;
    bool bCleanOutput = true;
    bool bRunAfterBuild = false;
};
