#include "Animation/AnimInstance.h"

#include "Asset/SkeletalMesh.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Stats.h"
#include "GameFramework/AActor.h"

#include <algorithm>

DEFINE_CLASS(UAnimInstance, UObject)

namespace
{
    bool IsValidAnimVariableName(const FName& Name)
    {
        return Name.IsValid() && Name != FName::None;
    }

    int32 FindRootMotionBoneIndex(const USkeletalMesh* Mesh, int32 PoseBoneCount)
    {
        if (!Mesh || PoseBoneCount <= 0)
        {
            return -1;
        }

        const TArray<FBoneInfo>& Bones = Mesh->GetBones();
        const int32 BoneCount = static_cast<int32>(Bones.size());
        const int32 SearchCount = std::min(BoneCount, PoseBoneCount);
        for (int32 BoneIndex = 0; BoneIndex < SearchCount; ++BoneIndex)
        {
            if (Bones[BoneIndex].ParentIndex < 0)
            {
                return BoneIndex;
            }
        }

        return -1;
    }

    bool IsExplicitRootMotionBoneName(const FName& BoneName)
    {
        return BoneName.IsValid() && BoneName != FName::None;
    }

    int32 ResolveRootMotionBoneIndex(
        const USkeletalMesh* Mesh,
        int32 PoseBoneCount,
        int32 RequestedBoneIndex,
        const FName& RequestedBoneName)
    {
        if (!Mesh || PoseBoneCount <= 0)
        {
            return -1;
        }

        const TArray<FBoneInfo>& Bones = Mesh->GetBones();
        const int32 BoneCount = static_cast<int32>(Bones.size());
        const int32 SearchCount = std::min(BoneCount, PoseBoneCount);
        if (SearchCount <= 0)
        {
            return -1;
        }

        if (RequestedBoneIndex >= 0)
        {
            return RequestedBoneIndex < SearchCount ? RequestedBoneIndex : -1;
        }

        if (IsExplicitRootMotionBoneName(RequestedBoneName))
        {
            for (int32 BoneIndex = 0; BoneIndex < SearchCount; ++BoneIndex)
            {
                if (Bones[BoneIndex].Name == RequestedBoneName)
                {
                    return BoneIndex;
                }
            }

            return -1;
        }

        return FindRootMotionBoneIndex(Mesh, PoseBoneCount);
    }

    void ResetRootMotionBoneToBindPose(TArray<FTransform>& InOutLocalPose, const USkeletalMesh* Mesh, int32 RootBoneIndex)
    {
        if (!Mesh || RootBoneIndex < 0 || RootBoneIndex >= static_cast<int32>(InOutLocalPose.size()))
        {
            return;
        }

        const TArray<FBoneInfo>& Bones = Mesh->GetBones();
        if (RootBoneIndex >= static_cast<int32>(Bones.size()))
        {
            return;
        }

        const FTransform BindRootTransform(Bones[RootBoneIndex].LocalBindTransform);
        InOutLocalPose[RootBoneIndex].SetTranslation(BindRootTransform.GetTranslation());
        InOutLocalPose[RootBoneIndex].SetRotation(BindRootTransform.GetRotation());
    }
}

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwningComponent)
{
    OwningComponent = InOwningComponent;
    NativeInitializeAnimation();
}

void UAnimInstance::Uninitialize()
{
    if (!OwningComponent)
    {
        return;
    }

    NativeUninitializeAnimation();
    OwningComponent = nullptr;
}

void UAnimInstance::NativeInitializeAnimation()
{
}

void UAnimInstance::NativeUninitializeAnimation()
{
}

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    (void)DeltaSeconds;
}

bool UAnimInstance::EvaluateAnimation(TArray<FTransform>& OutLocalPose)
{
    OutLocalPose.clear();
    return false;
}

USkeletalMeshComponent* UAnimInstance::GetOwningComponent() const
{
    return OwningComponent;
}

USkeletalMeshComponent* UAnimInstance::GetSkelMeshComponent() const
{
    return OwningComponent;
}

void UAnimInstance::SetAnimVariableFloat(const FName& Name, float Value)
{
    if (!IsValidAnimVariableName(Name))
    {
        return;
    }

    FloatVariables[Name] = Value;
}

bool UAnimInstance::GetAnimVariableFloat(const FName& Name, float& OutValue) const
{
    if (!IsValidAnimVariableName(Name))
    {
        return false;
    }

    const auto It = FloatVariables.find(Name);
    if (It == FloatVariables.end())
    {
        return false;
    }

    OutValue = It->second;
    return true;
}

float UAnimInstance::GetAnimVariableFloatOrDefault(const FName& Name, float DefaultValue) const
{
    float Value = DefaultValue;
    return GetAnimVariableFloat(Name, Value) ? Value : DefaultValue;
}

void UAnimInstance::SetAnimVariableBool(const FName& Name, bool Value)
{
    if (!IsValidAnimVariableName(Name))
    {
        return;
    }

    BoolVariables[Name] = Value;
}

bool UAnimInstance::GetAnimVariableBool(const FName& Name, bool& OutValue) const
{
    if (!IsValidAnimVariableName(Name))
    {
        return false;
    }

    const auto It = BoolVariables.find(Name);
    if (It == BoolVariables.end())
    {
        return false;
    }

    OutValue = It->second;
    return true;
}

bool UAnimInstance::GetAnimVariableBoolOrDefault(const FName& Name, bool DefaultValue) const
{
    bool Value = DefaultValue;
    return GetAnimVariableBool(Name, Value) ? Value : DefaultValue;
}

void UAnimInstance::SetAnimTrigger(const FName& Name)
{
    if (!IsValidAnimVariableName(Name))
    {
        return;
    }

    TriggerVariables.insert(Name);
}

void UAnimInstance::ResetAnimTrigger(const FName& Name)
{
    if (!IsValidAnimVariableName(Name))
    {
        return;
    }

    TriggerVariables.erase(Name);
}

bool UAnimInstance::IsAnimTriggerSet(const FName& Name) const
{
    return IsValidAnimVariableName(Name) && TriggerVariables.find(Name) != TriggerVariables.end();
}

bool UAnimInstance::ConsumeAnimTrigger(const FName& Name)
{
    if (!IsAnimTriggerSet(Name))
    {
        return false;
    }

    TriggerVariables.erase(Name);
    return true;
}

void UAnimInstance::ClearAnimTriggers()
{
    TriggerVariables.clear();
}

void UAnimInstance::ClearAnimVariables()
{
    FloatVariables.clear();
    BoolVariables.clear();
    ClearAnimTriggers();
}

void UAnimInstance::SetRootMotionMode(ERootMotionMode InMode)
{
    if (RootMotionMode == InMode)
    {
        return;
    }

    RootMotionMode = InMode;
    ClearRootMotionState();
}

ERootMotionMode UAnimInstance::GetRootMotionMode() const
{
    return RootMotionMode;
}

const FRootMotionDelta& UAnimInstance::GetLastExtractedRootMotion() const
{
    return LastExtractedRootMotion;
}

void UAnimInstance::SetRootMotionBoneIndex(int32 InBoneIndex)
{
    if (RootMotionBoneIndex == InBoneIndex && !IsExplicitRootMotionBoneName(RootMotionBoneName))
    {
        return;
    }

    RootMotionBoneIndex = InBoneIndex;
    RootMotionBoneName = FName();
    ClearRootMotionState();
}

int32 UAnimInstance::GetRootMotionBoneIndex() const
{
    return RootMotionBoneIndex;
}

void UAnimInstance::SetRootMotionBoneName(const FName& InBoneName)
{
    const FName NewBoneName = IsExplicitRootMotionBoneName(InBoneName) ? InBoneName : FName();
    if (RootMotionBoneName == NewBoneName && RootMotionBoneIndex < 0)
    {
        return;
    }

    RootMotionBoneName = NewBoneName;
    RootMotionBoneIndex = -1;
    ClearRootMotionState();
}

FName UAnimInstance::GetRootMotionBoneName() const
{
    return RootMotionBoneName;
}

void UAnimInstance::ProcessRootMotion(TArray<FTransform>& InOutLocalPose, bool bResetDelta)
{
    SCOPE_STAT("Anim.RootMotion");

    if (RootMotionMode == ERootMotionMode::Ignore)
    {
        ClearRootMotionState();
        return;
    }

    LastExtractedRootMotion = FRootMotionDelta();
    if (InOutLocalPose.empty())
    {
        bHasPreviousRootTransform = false;
        return;
    }

    USkeletalMeshComponent* Component = GetSkelMeshComponent();
    const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
    const int32 RootBoneIndex = ResolveRootMotionBoneIndex(
        Mesh,
        static_cast<int32>(InOutLocalPose.size()),
        RootMotionBoneIndex,
        RootMotionBoneName);
    if (RootBoneIndex < 0)
    {
        bHasPreviousRootTransform = false;
        return;
    }

    const FTransform CurrentRootTransform = InOutLocalPose[RootBoneIndex];
    if (!bHasPreviousRootTransform || bResetDelta)
    {
        // seek, loop, state 변경 직후에는 이전 root와 현재 root를 비교하면 큰 delta가 튈 수 있어 기준점만 잡습니다.
        PreviousRootTransform = CurrentRootTransform;
        bHasPreviousRootTransform = true;
        if (RootMotionMode == ERootMotionMode::ApplyToOwner)
        {
            ResetRootMotionBoneToBindPose(InOutLocalPose, Mesh, RootBoneIndex);
        }
        return;
    }

    const FTransform DeltaTransform = PreviousRootTransform.Inverse() * CurrentRootTransform;
    PreviousRootTransform = CurrentRootTransform;

    LastExtractedRootMotion.DeltaTransform = DeltaTransform;
    LastExtractedRootMotion.Translation = DeltaTransform.GetTranslation();
    LastExtractedRootMotion.Rotation = DeltaTransform.GetRotation();
    LastExtractedRootMotion.bHasRootMotion =
        !LastExtractedRootMotion.Translation.IsNearlyZero() || !LastExtractedRootMotion.Rotation.IsIdentity();

    if (RootMotionMode != ERootMotionMode::ApplyToOwner)
    {
        return;
    }

    if (Component)
    {
        if (AActor* Owner = Component->GetOwner())
        {
            if (!LastExtractedRootMotion.Translation.IsNearlyZero())
            {
                const FVector WorldDelta =
                    Component->GetWorldTransform().TransformVectorNoScale(LastExtractedRootMotion.Translation);
                Owner->AddActorWorldOffset(WorldDelta);
            }

            if (!LastExtractedRootMotion.Rotation.IsIdentity())
            {
                const FQuat ComponentRotation = Component->GetWorldTransform().GetRotation();
                const FQuat WorldDeltaRotation =
                    (ComponentRotation * LastExtractedRootMotion.Rotation * ComponentRotation.Inverse()).GetNormalized();
                FQuat OwnerRotation = FQuat::MakeFromEuler(Owner->GetActorRotation());
                OwnerRotation = (WorldDeltaRotation * OwnerRotation).GetNormalized();
                Owner->SetActorRotationQuat(OwnerRotation);
            }
        }
    }

    ResetRootMotionBoneToBindPose(InOutLocalPose, Mesh, RootBoneIndex);
}

void UAnimInstance::ClearRootMotionState()
{
    LastExtractedRootMotion = FRootMotionDelta();
    PreviousRootTransform = FTransform::Identity;
    bHasPreviousRootTransform = false;
}
