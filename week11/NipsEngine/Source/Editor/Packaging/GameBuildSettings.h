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
    FString StartupScene = "Asset/Scene/Default.scene";
    TArray<FString> IncludedScenes;
    FString PlayerControllerClass = "AGameJamPlayerController";
    FString OutputDirectory = "Builds/Windows/NipsGame";
    EGameBuildConfiguration Configuration = EGameBuildConfiguration::Development;
    bool bCleanOutput = true;
    bool bRunAfterBuild = false;
};
