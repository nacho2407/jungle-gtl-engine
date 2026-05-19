#include "Animation/AnimationRuntime.h"

#include "Asset/SkeletalMesh.h"

#include <algorithm>
#include <utility>

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

bool FAnimationRuntime::BlendLocalPoses(
    const TArray<FTransform>& PoseA,
    const TArray<FTransform>& PoseB,
    float Alpha,
    TArray<FTransform>& OutPose)
{
    if (PoseA.empty() || PoseA.size() != PoseB.size())
    {
        OutPose.clear();
        return false;
    }

    const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);

    TArray<FTransform> BlendedPose;
    BlendedPose.reserve(PoseA.size());
    for (size_t BoneIndex = 0; BoneIndex < PoseA.size(); ++BoneIndex)
    {
        const FTransform& TransformA = PoseA[BoneIndex];
        const FTransform& TransformB = PoseB[BoneIndex];

        // 위치와 스케일은 lerp, 회전은 slerp
        const FVector BlendedTranslation = FVector::Lerp(
            TransformA.GetTranslation(),
            TransformB.GetTranslation(),
            ClampedAlpha);
        const FQuat BlendedRotation = FQuat::Slerp(
            TransformA.GetRotation(),
            TransformB.GetRotation(),
            ClampedAlpha);
        const FVector BlendedScale = FVector::Lerp(
            TransformA.GetScale3D(),
            TransformB.GetScale3D(),
            ClampedAlpha);

        BlendedPose.push_back(FTransform(BlendedRotation, BlendedTranslation, BlendedScale));
    }

    OutPose = std::move(BlendedPose);
    return true;
}

bool FAnimationRuntime::HasMatchingBoneCount(const USkeletalMesh* Mesh, const TArray<FTransform>& LocalPose)
{
    return Mesh && Mesh->GetBones().size() == LocalPose.size();
}
