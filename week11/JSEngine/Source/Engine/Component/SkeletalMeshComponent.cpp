#include "SkeletalMeshComponent.h"

#include <utility>

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimationRuntime.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/Stats.h"
#include "Core/ResourceManager.h"
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
    TickAnimation(DeltaTime);
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

bool USkeletalMeshComponent::ApplyAnimationLocalPose(const TArray<FTransform>& LocalPose)
{
    SCOPE_STAT("Anim.ApplyPoseToComponent");

    if (!HasValidMesh())
    {
        return false;
    }

    if (!FAnimationRuntime::HasMatchingBoneCount(SkeletalMesh, LocalPose))
    {
        UE_LOG_WARNING(
            "[SkeletalMeshComponent] Animation local pose bone count mismatch. MeshBones=%d, PoseBones=%d",
            static_cast<int32>(SkeletalMesh->GetBones().size()),
            static_cast<int32>(LocalPose.size()));
        return false;
    }

    TArray<FMatrix> LocalMatrices;
    if (!FAnimationRuntime::ConvertLocalPoseToMatrices(LocalPose, LocalMatrices))
    {
        return false;
    }

    CurrentLocalPose = std::move(LocalMatrices);
    MarkPoseDirty();
    return true;
}

bool USkeletalMeshComponent::RefreshAnimationPose()
{
    if (AnimationMode == EAnimationMode::None || !AnimInstance)
    {
        return false;
    }

    TArray<FTransform> LocalPose;
    if (!AnimInstance->EvaluateAnimation(LocalPose))
    {
        return false;
    }

    if (!ApplyAnimationLocalPose(LocalPose))
    {
        return false;
    }

    EnsurePoseUpdated();
    return true;
}

void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping)
{
    UAnimSingleNodeInstance* SingleNodeInstance = EnsureSingleNodeInstance();
    if (!SingleNodeInstance)
    {
        UE_LOG_WARNING("[SkeletalMeshComponent] Failed to play animation because single node instance could not be created.");
        return;
    }

    if (!NewAnimToPlay)
    {
        UE_LOG_WARNING("[SkeletalMeshComponent] PlayAnimation called with null animation asset.");
    }

    SingleNodeInstance->SetAnimationAsset(NewAnimToPlay);
    SingleNodeInstance->SetLooping(bLooping);
    SingleNodeInstance->Play();
}

void USkeletalMeshComponent::SetAnimation(UAnimationAsset* NewAnimToPlay)
{
    UAnimSingleNodeInstance* SingleNodeInstance = EnsureSingleNodeInstance();
    if (!SingleNodeInstance)
    {
        return;
    }

    SingleNodeInstance->SetAnimationAsset(NewAnimToPlay);
    SingleNodeInstance->SetPosition(0.0f, false);
    RefreshAnimationPose();
}

UAnimationAsset* USkeletalMeshComponent::GetAnimation() const
{
    const UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
    return SingleNodeInstance ? SingleNodeInstance->GetAnimationAsset() : nullptr;
}

void USkeletalMeshComponent::Play()
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->Play();
    }
}

void USkeletalMeshComponent::Pause()
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->Pause();
    }
}

void USkeletalMeshComponent::Stop()
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->Stop();
    }
}

void USkeletalMeshComponent::SetPosition(float TimeSeconds, bool bFireNotifies)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetPosition(TimeSeconds, bFireNotifies);
    }
}

float USkeletalMeshComponent::GetPosition() const
{
    const UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
    return SingleNodeInstance ? SingleNodeInstance->GetPosition() : 0.0f;
}

void USkeletalMeshComponent::SetPlayRate(float InPlayRate)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetPlayRate(InPlayRate);
    }
}

float USkeletalMeshComponent::GetPlayRate() const
{
    const UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
    return SingleNodeInstance ? SingleNodeInstance->GetPlayRate() : 1.0f;
}

void USkeletalMeshComponent::SetLooping(bool bInLooping)
{
    if (UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance())
    {
        SingleNodeInstance->SetLooping(bInLooping);
    }
}

bool USkeletalMeshComponent::IsLooping() const
{
    const UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
    return SingleNodeInstance ? SingleNodeInstance->IsLooping() : false;
}

bool USkeletalMeshComponent::IsPlaying() const
{
    const UAnimSingleNodeInstance* SingleNodeInstance = GetSingleNodeInstance();
    return SingleNodeInstance ? SingleNodeInstance->IsPlaying() : false;
}

float USkeletalMeshComponent::GetPlayLength() const
{
    const UAnimationAsset* Animation = GetAnimation();
    return Animation ? Animation->GetPlayLength() : 0.0f;
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
    SetPosition(Time, false);
    RefreshAnimationPose();
}

void USkeletalMeshComponent::TickAnimation(float DeltaTime)
{
    if (AnimationMode == EAnimationMode::None || !AnimInstance)
    {
        return;
    }

    SCOPE_STAT("Anim.ComponentTick");

    AnimInstance->NativeUpdateAnimation(DeltaTime);

    TArray<FTransform> LocalPose;
    if (AnimInstance->EvaluateAnimation(LocalPose))
    {
        ApplyAnimationLocalPose(LocalPose);
    }
}

void USkeletalMeshComponent::PlayAnim(bool bLoop)
{
    SetLooping(bLoop);
    Play();
}

void USkeletalMeshComponent::StopAnim()
{
    Stop();
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
        AnimInstance->Uninitialize();
        UObjectManager::Get().DestroyObject(AnimInstance);
    }
    AnimInstance = nullptr;
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetSingleNodeInstance() const
{
    return Cast<UAnimSingleNodeInstance>(AnimInstance);
}

UAnimSingleNodeInstance* USkeletalMeshComponent::EnsureSingleNodeInstance()
{
    if (AnimationMode != EAnimationMode::SingleNode)
    {
        SetAnimationMode(EAnimationMode::SingleNode);
    }
    else if (!GetSingleNodeInstance())
    {
        RecreateAnimInstance();
    }

    return GetSingleNodeInstance();
}

void USkeletalMeshComponent::ResetToBindPose()
{
    InitializePoseFromBindPose();
    MarkPoseDirty();
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return;
    }

    CurrentLocalPose[BoneIndex] = NewLocalTransform;
    MarkPoseDirty();
}

const FMatrix& USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return Identity;
    }

    return CurrentLocalPose[BoneIndex];
}

FMatrix USkeletalMeshComponent::GetBoneGlobalTransform(int32 BoneIndex) const
{
    const_cast<USkeletalMeshComponent*>(this)->EnsurePoseUpdated();

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

    const int32 ParentIndex = Bones[BoneIndex].ParentIndex;

    FMatrix ParentGlobalTransform;
    if (ParentIndex >= 0)
    {
        EnsurePoseUpdated();
        ParentGlobalTransform = CurrentGlobalPose[ParentIndex] * GetWorldMatrix();
    }
    else
    {
        ParentGlobalTransform = GetWorldMatrix();
    }

    const FMatrix NewLocalTransform = NewGlobalTransform * ParentGlobalTransform.GetInverse();
    SetBoneLocalTransform(BoneIndex, NewLocalTransform);
}
