#pragma once

#include "Core/CoreTypes.h"
#include "Asset/AssetRegistry.h"
#include "Animation/SkeletonTypes.h"

class USkeleton;

class FSkeletonManager
{
public:
    static FSkeletonManager& Get();

    USkeleton* LoadSkeleton(const FString& PackagePath);

    bool SaveSkeleton(USkeleton* Skeleton, const FString& PackagePath, const FString& SourcePath);

    const TArray<FAssetListItem>& GetAvailableSkeletonFiles();
    void ScanSkeletonAssets();

    USkeleton* FindSkeletonByAssetGuid(const FString& SkeletonAssetGuid);

    static FString GetSkeletonPackagePath(const FString& SourcePath);
    static FString BuildCompatibilitySignature(const FReferenceSkeleton& RefSkeleton);
    static FString MakeSkeletonAssetGuid(const FString& PackagePath, const FString& CompatibilitySignature);

    static FSkeletonCompatibilityReport CheckCompatibility(
        const FSkeletonBinding& A,
        const FSkeletonBinding& B,
        const USkeleton* LoadedA = nullptr,
        const USkeleton* LoadedB = nullptr);

    static bool AreReferenceSkeletonsSameStructure(
        const FReferenceSkeleton& A,
        const FReferenceSkeleton& B,
        FSkeletonCompatibilityReport* OutReport = nullptr);

private:
    FSkeletonManager() = default;

private:
    TMap<FString, USkeleton*> SkeletonCaches;
    TArray<FAssetListItem> AvailableSkeletonFiles;
};
