#pragma once

#include "Core/CoreTypes.h"

class USkeleton;

class FSkeletonManager
{
public:
    static FSkeletonManager& Get();

    USkeleton* LoadSkeleton(const FString& PackagePath);

    bool SaveSkeleton(USkeleton* Skeleton, const FString& PackagePath, const FString& SourcePath);

    static FString GetSkeletonPackagePath(const FString& SourcePath);

private:
    FSkeletonManager() = default;

private:
    TMap<FString, USkeleton*> SkeletonCaches;
};
