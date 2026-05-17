#pragma once

#include "Core/CoreTypes.h"
#include "Asset/AssetRegistry.h"

class UAnimSequence;

struct FAnimationImportRequest
{
    FString SourceFbxPath;
    FString TargetSkeletonPath = "None";
    FString DestinationDirectory;
    bool    bAllowTargetExtraBones   = false;
    bool    bOverwriteExistingAssets = false;
};

class FAnimationManager
{
public:
    static FAnimationManager& Get();

    UAnimSequence* LoadAnimation(const FString& PackagePath);

    bool SaveAnimation(UAnimSequence* Sequence, const FString& PackagePath, const FString& SourcePath);

    bool ImportAnimationForSkeleton(const FAnimationImportRequest& Request, TArray<UAnimSequence*>* OutSequences = nullptr);

    const TArray<FAssetListItem>& GetAvailableAnimationFiles() const
    {
        return AvailableAnimationFiles;
    }

    static FString GetAnimationPath(const FString& SourcePath, const FString& AnimationName);
    static FString GetAnimationPathForSkeleton(const FString& SourcePath, const FString& AnimationName, const FString& TargetSkeletonPath);

private:
    FAnimationManager() = default;

private:
    TMap<FString, UAnimSequence*> AnimationCaches;
    TArray<FAssetListItem> AvailableAnimationFiles;
};
