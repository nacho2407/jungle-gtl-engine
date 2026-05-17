#include "SkeletalMeshComponent.h"

#include "Animation/AnimDataModel.h"
#include "Animation/AnimSequence.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

DEFINE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
REGISTER_FACTORY(USkeletalMeshComponent)

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    USkinnedMeshComponent::TickComponent(DeltaTime);

    TickAnimation(DeltaTime);

	// Pose가 바뀐 경우에만 실제 CPU skinning이 수행(dirty flag 이용)
    EnsureSkinningUpdated();
}

void USkeletalMeshComponent::ResetToBindPose()
{
    AnimationSequence = nullptr;
    AnimationTime = 0.0f;
    bAnimationPlaying = false;
    bAnimationLooping = false;

    InitializePoseFromBindPose();
    MarkSkinningDirty();
    MarkBoundsDirty();
}

void USkeletalMeshComponent::SetAnimation(UAnimSequence* InSequence)
{
    AnimationSequence = InSequence;
    AnimationTime = 0.0f;
    bAnimationPlaying = false;

    if (!ApplyAnimationPose())
    {
        ResetToBindPose();
    }
}

bool USkeletalMeshComponent::SetAnimSequence(const FString& SourceFbxPath, const FString& AnimStackName)
{
    if (!SkeletalMesh)
    {
        return false;
    }

    UAnimSequence* LoadedSequence = FResourceManager::Get().LoadAnimSequence(
        SourceFbxPath,
        SkeletalMesh->GetAssetPathFileName(),
        AnimStackName);

    if (!LoadedSequence)
    {
        return false;
    }

    SetAnimation(LoadedSequence);
    return true;
}

void USkeletalMeshComponent::SetAnimationTime(float Time)
{
    AnimationTime = std::max(0.0f, Time);
    ApplyAnimationPose();
}

void USkeletalMeshComponent::TickAnimation(float DeltaTime)
{
    if (!bAnimationPlaying || !AnimationSequence || !AnimationSequence->DataModel)
    {
        return;
    }

    const float SequenceLength = AnimationSequence->DataModel->SequenceLength;
    AnimationTime += DeltaTime * AnimationPlayRate;

    if (SequenceLength > 0.0f)
    {
        if (bAnimationLooping)
        {
            AnimationTime = std::fmod(AnimationTime, SequenceLength);
            if (AnimationTime < 0.0f)
            {
                AnimationTime += SequenceLength;
            }
        }
        else if (AnimationTime >= SequenceLength)
        {
            AnimationTime = SequenceLength;
            bAnimationPlaying = false;
        }
    }

    ApplyAnimationPose();
}

void USkeletalMeshComponent::PlayAnim(bool bLoop)
{
    bAnimationLooping = bLoop;
    bAnimationPlaying = AnimationSequence != nullptr;
}

void USkeletalMeshComponent::StopAnim()
{
    bAnimationPlaying = false;
}

bool USkeletalMeshComponent::ApplyAnimationPose()
{
    if (!AnimationSequence || !SkeletalMesh)
    {
        return false;
    }

    TArray<FMatrix> EvaluatedLocalPose;
    if (!AnimationSequence->GetBonePose(AnimationTime, SkeletalMesh, EvaluatedLocalPose))
    {
        return false;
    }

    CurrentLocalPose = EvaluatedLocalPose;
    UpdateCurrentGlobalPose();
    MarkSkinningDirty();
    MarkBoundsDirty();
    return true;
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return;
    }

    CurrentLocalPose[BoneIndex] = NewLocalTransform;
    UpdateCurrentGlobalPose();
    MarkSkinningDirty();
}

const FMatrix& USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	// fallback은 identity
    static const FMatrix Identity = FMatrix::Identity;

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return Identity;
    }

    return CurrentLocalPose[BoneIndex];
}

FMatrix USkeletalMeshComponent::GetBoneGlobalTransform(int32 BoneIndex) const
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentGlobalPose.size()))
    {
        return FMatrix::Identity;
    }

    return CurrentGlobalPose[BoneIndex] * GetWorldMatrix();
}

void USkeletalMeshComponent::SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return;
    }

    if (!SkeletalMesh)
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetBones();
    if (BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return;
    }

    int32 ParentIndex = Bones[BoneIndex].ParentIndex;

    FMatrix ParentGlobalTransform;
    if (ParentIndex >= 0)
    {
        ParentGlobalTransform = CurrentGlobalPose[ParentIndex] * GetWorldMatrix();
    }
    else
    {
        ParentGlobalTransform = GetWorldMatrix();
    }

    // Local = Global * ParentGlobal.Inverse
    FMatrix NewLocalTransform = NewGlobalTransform * ParentGlobalTransform.GetInverse();
    SetBoneLocalTransform(BoneIndex, NewLocalTransform);
}
