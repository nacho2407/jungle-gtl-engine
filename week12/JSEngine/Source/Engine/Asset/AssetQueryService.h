#pragma once

#include "Core/CoreMinimal.h"

class FAssetQueryService
{
public:
    static bool NormalizeAssetPath(const FString& Path, FString& OutRelativePath);
    static bool Exists(const FString& Path);

    static TArray<FString> GetTexturePaths();
    static TArray<FString> GetStaticMeshPaths();
    static TArray<FString> GetMaterialPaths();
    static TArray<FString> GetCurvePaths();
    static TArray<FString> GetScenePaths();
    static TArray<FString> GetSoundPaths();
};
