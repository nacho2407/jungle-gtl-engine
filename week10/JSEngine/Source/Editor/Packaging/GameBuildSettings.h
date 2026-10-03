#pragma once

#include "Core/CoreMinimal.h"

enum class EGameBuildConfiguration : uint8
{
    Development,
    Shipping
};

struct FGameBuildSettings
{
    FString GameName = "JSEngineGame";
    FString StartupScene = "Asset/Scene/Main.Scene";
    TArray<FString> IncludedScenes;
    FString GameModeClass = "AGameModeBase";
    FString PlayerControllerClass = "APlayerController";
    FString DefaultPawnClass = "ADefaultPawn";
    FString DefaultPawnPrefabPath;
    FString OutputDirectory = "Builds/Windows/JSEngineGame";
    FString IconPath;
    FString SplashImagePath;
    float SplashMinSeconds = 3.0f;
    EGameBuildConfiguration Configuration = EGameBuildConfiguration::Development;
    bool bCleanOutput = true;
    bool bRunAfterBuild = false;
};
