#include "Animation/AnimationRuntime.h"

#include "Asset/SkeletalMesh.h"

bool FAnimationRuntime::BuildBindLocalPoseFromMesh(const USkeletalMesh* Mesh, TArray<FTransform>& OutLocalPose)
{
    OutLocalPose.clear();

    if (!Mesh)
    {
        return false;
    }

    const TArray<FBoneInfo>& Bones = Mesh->GetBones();
    if (Bones.empty())
    {
        return false;
    }

    OutLocalPose.reserve(Bones.size());
    for (const FBoneInfo& Bone : Bones)
    {
        OutLocalPose.push_back(FTransform(Bone.LocalBindTransform));
    }

    return true;
}

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
