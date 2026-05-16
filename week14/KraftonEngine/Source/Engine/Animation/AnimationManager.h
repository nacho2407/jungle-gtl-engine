#pragma once

#include "Core/CoreTypes.h"
#include "Asset/AssetRegistry.h"

class UAnimSequence;

class FAnimationManager
{
public:
    static FAnimationManager& Get();

    UAnimSequence* LoadAnimation(const FString& PackagePath);

    bool SaveAnimation(UAnimSequence* Sequence, const FString& PackagePath, const FString& SourcePath);

    const TArray<FAssetListItem>& GetAvailableAnimationFiles() const
    {
        return AvailableAnimationFiles;
    }

    static FString GetAnimationPackagePath(const FString& SourcePath, const FString& AnimationName);

private:
    FAnimationManager() = default;

private:
    TMap<FString, UAnimSequence*> AnimationCaches;
    TArray<FAssetListItem> AvailableAnimationFiles;
};
