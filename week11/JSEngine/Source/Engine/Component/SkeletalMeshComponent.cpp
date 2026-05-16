#include "SkeletalMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Core/Logging/Log.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
REGISTER_FACTORY(USkeletalMeshComponent)

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    DestroyAnimInstance();
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    USkinnedMeshComponent::TickComponent(DeltaTime);

    if (AnimationMode != EAnimationMode::None && AnimInstance)
    {
        AnimInstance->NativeUpdateAnimation(DeltaTime);
        // animation pose 평가와 적용은 이후 단계에서 추가
    }

	// Pose가 바뀐 경우에만 실제 CPU skinning이 수행(dirty flag 이용)
    EnsureSkinningUpdated();
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InMode)
{
    if (AnimationMode == InMode)
    {
        return;
    }

    AnimationMode = InMode;
    RecreateAnimInstance();
}

EAnimationMode USkeletalMeshComponent::GetAnimationMode() const
{
    return AnimationMode;
}

void USkeletalMeshComponent::SetAnimInstanceClass(const FString& InClassName)
{
    if (AnimInstanceClassName == InClassName)
    {
        return;
    }

    AnimInstanceClassName = InClassName;
    if (AnimationMode == EAnimationMode::AnimInstance)
    {
        RecreateAnimInstance();
    }
}

UAnimInstance* USkeletalMeshComponent::GetAnimInstance() const
{
    return AnimInstance;
}

void USkeletalMeshComponent::RecreateAnimInstance()
{
    DestroyAnimInstance();

    UObject* NewObject = nullptr;
    if (AnimationMode == EAnimationMode::SingleNode)
    {
        NewObject = FObjectFactory::Get().Create("UAnimSingleNodeInstance");
    }
    else if (AnimationMode == EAnimationMode::AnimInstance)
    {
        if (AnimInstanceClassName.empty())
        {
            UE_LOG_WARNING("[SkeletalMeshComponent] AnimInstanceClassName is empty.");
            return;
        }

        NewObject = FObjectFactory::Get().Create(AnimInstanceClassName);
        if (!NewObject)
        {
            UE_LOG_WARNING("[SkeletalMeshComponent] Failed to create AnimInstance class: %s", AnimInstanceClassName.c_str());
            return;
        }
    }
    else
    {
        return;
    }

    if (!NewObject)
    {
        UE_LOG_WARNING("[SkeletalMeshComponent] Failed to create anim instance for mode: %d", static_cast<int32>(AnimationMode));
        return;
    }

    AnimInstance = Cast<UAnimInstance>(NewObject);
    if (!AnimInstance)
    {
        UE_LOG_WARNING("[SkeletalMeshComponent] Created object is not UAnimInstance: %s", NewObject->GetTypeInfo()->name);
        UObjectManager::Get().DestroyObject(NewObject);
        return;
    }

    AnimInstance->Initialize(this);
}

void USkeletalMeshComponent::DestroyAnimInstance()
{
    if (!AnimInstance)
    {
        return;
    }

    if (UObjectManager::Get().ContainsObject(AnimInstance))
    {
        UObjectManager::Get().DestroyObject(AnimInstance);
    }
    AnimInstance = nullptr;
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
