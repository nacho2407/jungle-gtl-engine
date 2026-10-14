#pragma once

#include "Core/Types/CoreTypes.h"
#include "Asset/AssetRegistry.h"
#include "Animation/Skeleton/SkeletonTypes.h"

class USkeleton;
class FReferenceCollector;

class FSkeletonManager
{
public:
    static FSkeletonManager& Get();

    USkeleton* LoadSkeleton(const FString& PackagePath);

    bool SaveSkeleton(USkeleton* Skeleton, const FString& PackagePath, const FString& SourcePath);

    const TArray<FAssetListItem>& GetAvailableSkeletonFiles();
    void                          ScanSkeletonAssets();

    USkeleton* FindSkeletonByAssetGuid(const FString& SkeletonAssetGuid);

    static FString GetSkeletonPackagePath(const FString& SourcePath);
    static FString BuildCompatibilitySignature(const FReferenceSkeleton& RefSkeleton);
    static FString MakeSkeletonAssetGuid(const FString& PackagePath, const FString& CompatibilitySignature);

    static FSkeletonCompatibilityReport CheckCompatibility(
        const FSkeletonBinding& A,
        const FSkeletonBinding& B,
        const USkeleton*        LoadedA = nullptr,
        const USkeleton*        LoadedB = nullptr
        );

    static bool AreSkeletonsSameStructure(const FReferenceSkeleton& A, const FReferenceSkeleton& B, FSkeletonCompatibilityReport* OutReport = nullptr);

    static bool BuildBoneRemapByName(
        const FReferenceSkeleton&     SourceSkeleton,
        const FReferenceSkeleton&     TargetSkeleton,
        FSkeletonBoneRemap&           OutRemap,
        FSkeletonCompatibilityReport* OutReport            = nullptr,
        bool                          bRequireExactBoneSet = true
        );

	// GC
	void AddReferencedObjects(FReferenceCollector& Collector);

private:
    FSkeletonManager() = default;

private:
    TMap<FString, USkeleton*> SkeletonCaches;
    TArray<FAssetListItem>    AvailableSkeletonFiles;
};
