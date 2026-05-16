#pragma once

#include "Object/Object.h"
#include "Animation/SkeletonTypes.h"

class USkeleton : public UObject
{
public:
    DECLARE_CLASS(USkeleton, UObject)

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

    const FString& GetSkeletonGuid() const
    {
        return SkeletonGuid;
    }

    void SetSkeletonGuid(const FString& InGuid)
    {
        SkeletonGuid = InGuid;
    }

    const FReferenceSkeleton& GetReferenceSkeleton() const
    {
        return ReferenceSkeleton;
    }

    FReferenceSkeleton& GetMutableReferenceSkeleton()
    {
        return ReferenceSkeleton;
    }

    int32 FindBoneIndex(const FString& BoneName) const
    {
        return ReferenceSkeleton.FindBoneIndex(BoneName);
    }

private:
    FString            AssetPathFileName = "None";
    FString            SkeletonGuid;
    FReferenceSkeleton ReferenceSkeleton;
};
