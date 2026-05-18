#pragma once

#include "Object/Object.h"
#include "Animation/SkeletonTypes.h"

#include "Source/Engine/Animation/Skeleton.generated.h"

UCLASS()
class USkeleton : public UObject
{
public:
	GENERATED_BODY()
    USkeleton()           = default;
    ~USkeleton() override = default;

    void Serialize(FArchive& Ar) override;

    const FString& GetAssetPathFileName() const
    {
        return AssetPathFileName;
    }

    void SetAssetPathFileName(const FString& InPath)
    {
        AssetPathFileName = InPath;
    }

    const FString& GetSkeletonAssetGuid() const
    {
        return SkeletonAssetGuid;
    }

    void SetSkeletonAssetGuid(const FString& InGuid)
    {
        SkeletonAssetGuid = InGuid;
    }

    const FString& GetCompatibilitySignature() const
    {
        return CompatibilitySignature;
    }

    void SetCompatibilitySignature(const FString& InSignature)
    {
        CompatibilitySignature = InSignature;
    }

    FSkeletonBinding GetSkeletonBinding() const
    {
        FSkeletonBinding Binding;
        Binding.SkeletonPath = AssetPathFileName;
        Binding.SkeletonAssetGuid = SkeletonAssetGuid;
        Binding.CompatibilitySignature = CompatibilitySignature;
        return Binding;
    }

    const FReferenceSkeleton& GetReferenceSkeleton() const
    {
        return ReferenceSkeleton;
    }

    FReferenceSkeleton& GetMutableReferenceSkeleton()
    {
        return ReferenceSkeleton;
    }

    void RebuildBoneNameCache();

    int32 FindBoneIndex(const FString& BoneName) const;

private:
    FString            AssetPathFileName = "None";
    FString            SkeletonAssetGuid;
    FString            CompatibilitySignature;
    FReferenceSkeleton ReferenceSkeleton;
    TMap<FString, int32> BoneNameToIndex;
};
