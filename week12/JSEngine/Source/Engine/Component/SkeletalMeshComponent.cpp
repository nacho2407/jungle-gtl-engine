#include "SkeletalMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
REGISTER_FACTORY(USkeletalMeshComponent)

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
    USkinnedMeshComponent::Serialize(Ar);
}

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USkinnedMeshComponent::GetEditableProperties(OutProps);
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
    USkinnedMeshComponent::PostEditProperty(PropertyName);
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    USkinnedMeshComponent::TickComponent(DeltaTime);

    if (AnimInstance)
    {
        AnimInstance->NativeUpdateAnimation(DeltaTime);

        FPoseContext PoseContext;
        PoseContext.LocalPose.resize(CurrentLocalPose.size());
        // PoseContext.LocalPose = CurrentLocalPose; // Instance 디버그용
        if (AnimInstance->EvaluatePose(PoseContext))
        {
            ApplyAnimationPose(PoseContext);
        }
    }

	// Pose가 바뀐 경우에만 실제 CPU skinning이 수행(dirty flag 이용)
    EnsureSkinningUpdated();
}

void USkeletalMeshComponent::ApplyAnimationPose(const FPoseContext& PoseContext)
{
    if (PoseContext.LocalPose.size() != CurrentLocalPose.size())
    {
        return;
    }

    CurrentLocalPose = PoseContext.LocalPose;
    MarkSkinningDirty();
}

void USkeletalMeshComponent::ResetToBindPose()
{
    InitializePoseFromBindPose();
    MarkSkinningDirty();
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

void USkeletalMeshComponent::PlayAnimation(UAnimSequenceBase* NewAnimToPlay, bool bLooping)
{
    UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance);
    if (!SingleNode)
    {
        SingleNode = new UAnimSingleNodeInstance();
        SingleNode->Initialize(this);

        AnimInstance = SingleNode;
    }

    SingleNode->SetAnimation(NewAnimToPlay);
	SingleNode->Play(bLooping);
}

void USkeletalMeshComponent::SetAnimation(UAnimSequenceBase* NewAnimToPlay)
{
    if (auto* SingleNode = dynamic_cast<UAnimSingleNodeInstance*>(AnimInstance))
    {
        SingleNode->SetAnimation(NewAnimToPlay);
    }
}

void USkeletalMeshComponent::Play(bool bLooping)
{
    if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
		SingleNode->Play(bLooping);
    }
}

void USkeletalMeshComponent::Stop()
{
    if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->Stop();
    }
}

void USkeletalMeshComponent::Pause()
{
    if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->Pause();
    }
}

void USkeletalMeshComponent::SetPlayRate(float InPlayRate)
{
    if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetPlayRate(InPlayRate);
    }
}

void USkeletalMeshComponent::SetAnimationPosition(float InTime)
{
    if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
		SingleNode->SetPosition(InTime);
    }
}

void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    UE_LOG("[AnimNotify] %s triggered at %.3f", Notify.NotifyName.ToString().c_str(), Notify.TriggerTime);
    OnAnimNotifyDelegate.Broadcast(this, Notify);

    if (AActor* OwnerActor = GetOwner())
    {
        // OwnerActor->HandleAnimNotify(Notify);
    }
}