#include "Animation/AnimationRuntime.h"

#include "Asset/SkeletalMesh.h"

bool FAnimationRuntime::ConvertLocalPoseToMatrices(
    const TArray<FTransform>& LocalPose,
    TArray<FMatrix>& OutLocalMatrices)
{
    OutLocalMatrices.clear();

    if (LocalPose.empty())
    {
        return false;
    }

    OutLocalMatrices.reserve(LocalPose.size());
    for (const FTransform& BoneTransform : LocalPose)
    {
        OutLocalMatrices.push_back(BoneTransform.ToMatrixWithScale());
    }

    return true;
}

bool FAnimationRuntime::HasMatchingBoneCount(const USkeletalMesh* Mesh, const TArray<FTransform>& LocalPose)
{
    return Mesh && Mesh->GetBones().size() == LocalPose.size();
}
