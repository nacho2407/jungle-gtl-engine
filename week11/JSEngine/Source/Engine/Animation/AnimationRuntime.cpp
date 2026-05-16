#include "Animation/AnimationRuntime.h"

#include "Asset/SkeletalMesh.h"

#include <cmath>

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

bool FAnimationRuntime::BuildDebugOscillatingLocalPose(
    const USkeletalMesh* Mesh,
    float TimeSeconds,
    TArray<FTransform>& OutLocalPose)
{
    if (!BuildBindLocalPoseFromMesh(Mesh, OutLocalPose))
    {
        return false;
    }

    const TArray<FBoneInfo>& Bones = Mesh->GetBones();
    int32 DebugBoneIndex = -1;
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        if (Bones[BoneIndex].ParentIndex >= 0)
        {
            DebugBoneIndex = BoneIndex;
            break;
        }
    }

    if (DebugBoneIndex < 0)
    {
        return true;
    }

    const float AngleRadians = 0.25f * static_cast<float>(std::sin(static_cast<double>(TimeSeconds) * 2.0));
    const FQuat DeltaRotation(FVector::UpVector, AngleRadians);
    const FQuat NewRotation = OutLocalPose[DebugBoneIndex].GetRotation() * DeltaRotation;
    OutLocalPose[DebugBoneIndex].SetRotation(NewRotation);
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
