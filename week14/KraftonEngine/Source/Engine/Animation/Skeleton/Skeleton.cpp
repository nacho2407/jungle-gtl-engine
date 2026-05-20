#include "Animation/Skeleton/Skeleton.h"
void USkeleton::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    Ar << AssetPathFileName;
    Ar << SkeletonAssetGuid;
    Ar << CompatibilitySignature;
    Ar << ReferenceSkeleton;

    if (Ar.IsLoading())
    {
        RebuildBoneNameCache();
    }
}

void USkeleton::RebuildBoneNameCache()
{
    BoneNameToIndex.clear();

    for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.GetNumBones(); ++BoneIndex)
    {
        BoneNameToIndex[ReferenceSkeleton.Bones[BoneIndex].Name] = BoneIndex;
    }
}

int32 USkeleton::FindBoneIndex(const FString& BoneName) const
{
    auto It = BoneNameToIndex.find(BoneName);
    if (It != BoneNameToIndex.end())
    {
        return It->second;
    }

    return ReferenceSkeleton.FindBoneIndex(BoneName);
}
