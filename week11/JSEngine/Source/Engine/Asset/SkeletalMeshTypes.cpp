#include "SkeletalMeshTypes.h"
DEFINE_CLASS(USkeleton, UObject)
//Instead of performing a linear search for bone lookup every time
//it reliably finds BoneName -> BoneIndex during animation track import/evaluate
void FReferenceSkeleton::RebuildNameToIndex()
{
    BoneNameToIndex.clear();

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(RefBones.size()); ++BoneIndex)
    {
        BoneNameToIndex[FName(RefBones[BoneIndex].Name.c_str())] = BoneIndex;
    }
}
int32 FReferenceSkeleton::FindBoneIndex(const FName& BoneName) const
{
    auto It = BoneNameToIndex.find(BoneName);
    return It != BoneNameToIndex.end() ? It->second : -1;
}