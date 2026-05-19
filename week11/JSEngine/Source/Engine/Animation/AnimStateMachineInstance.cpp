#include "Animation/AnimStateMachineInstance.h"

#include "Animation/AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimStateMachine.h"
#include "Asset/SkeletalMesh.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

DEFINE_CLASS(UAnimStateMachineInstance, UAnimInstance)
REGISTER_FACTORY(UAnimStateMachineInstance)

namespace
{
constexpr float AnimationTimeEpsilon = 1.0e-6f;
constexpr float ConditionFloatEpsilon = 1.0e-4f;

const char* ToConditionTypeDebugName(EAnimConditionType Type)
{
    switch (Type)
    {
    case EAnimConditionType::Bool:
        return "Bool";
    case EAnimConditionType::Float:
        return "Float";
    case EAnimConditionType::LuaFunction:
        return "LuaFunction";
    case EAnimConditionType::None:
    default:
        return "None";
    }
}

float ClampStateTime(float Time, float PlayLength)
{
    if (PlayLength <= 0.0f)
    {
        return 0.0f;
    }

    return std::clamp(Time, 0.0f, PlayLength);
}

float WrapStateTime(float Time, float PlayLength)
{
    if (PlayLength <= 0.0f)
    {
        return 0.0f;
    }

    float WrappedTime = std::fmod(Time, PlayLength);
    if (WrappedTime < 0.0f)
    {
        WrappedTime += PlayLength;
    }

    return WrappedTime;
}

const FAnimTransitionDesc* ChooseHigherPriorityTransition(
    const FAnimTransitionDesc* CurrentBest,
    const FAnimTransitionDesc* Candidate)
{
    if (!Candidate)
    {
        return CurrentBest;
    }

    if (!CurrentBest || Candidate->Priority > CurrentBest->Priority)
    {
        return Candidate;
    }

    return CurrentBest;
}
}

void UAnimStateMachineInstance::NativeInitializeAnimation()
{
}

void UAnimStateMachineInstance::NativeUninitializeAnimation()
{
    ResetRuntime();
}

bool UAnimStateMachineInstance::LoadStateMachine(const FString& Path)
{
    UAnimStateMachine* LoadedStateMachine = FResourceManager::Get().LoadAnimStateMachine(Path);
    return SetStateMachine(LoadedStateMachine);
}

bool UAnimStateMachineInstance::SetStateMachine(UAnimStateMachine* InStateMachine)
{
    ResetRuntime();

    if (!InStateMachine)
    {
        return false;
    }

    USkeletalMeshComponent* Component = GetSkelMeshComponent();
    const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
    if (!Mesh)
    {
        UE_LOG_WARNING("[AnimStateMachineInstance] Cannot set state machine without skeletal mesh.");
        return false;
    }

    const FString TargetSkeletalMeshPath = Mesh->GetAssetPathFileName();
    if (TargetSkeletalMeshPath.empty())
    {
        UE_LOG_WARNING("[AnimStateMachineInstance] Target skeletal mesh path is empty.");
        return false;
    }

    StateMachineAsset = InStateMachine;
    Desc = InStateMachine->GetDesc();

    RuntimeStates.reserve(Desc.States.size());
    for (const FAnimStateDesc& StateDesc : Desc.States)
    {
        UAnimSequence* Sequence = FResourceManager::Get().LoadAnimSequence(
            StateDesc.Animation.SourceFbxPath,
            TargetSkeletalMeshPath,
            StateDesc.Animation.AnimStackName);

        if (!Sequence)
        {
            UE_LOG_WARNING(
                "[AnimStateMachineInstance] Failed to load state sequence. State=%s Source=%s Target=%s Stack=%s",
                StateDesc.Name.ToString().c_str(),
                StateDesc.Animation.SourceFbxPath.c_str(),
                TargetSkeletalMeshPath.c_str(),
                StateDesc.Animation.AnimStackName.c_str());
            ResetRuntime();
            return false;
        }

        FAnimStateRuntime RuntimeState;
        RuntimeState.Desc = &StateDesc;
        RuntimeState.Sequence = Sequence;
        RuntimeStates.push_back(RuntimeState);
    }

    RuntimeTransitions.reserve(Desc.Transitions.size());
    for (const FAnimTransitionDesc& TransitionDesc : Desc.Transitions)
    {
        RuntimeTransitions.push_back(&TransitionDesc);
    }

    CurrentStateIndex = FindRuntimeStateIndexByName(Desc.EntryState);
    if (CurrentStateIndex < 0)
    {
        UE_LOG_WARNING(
            "[AnimStateMachineInstance] Entry state not found. EntryState=%s",
            Desc.EntryState.ToString().c_str());
        ResetRuntime();
        return false;
    }

    PreviousStateIndex = -1;
    TargetStateIndex = -1;
    ActiveTransition = FAnimTransitionRuntime();
    return true;
}

void UAnimStateMachineInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    if (ActiveTransition.bActive)
    {
        TargetStateIndex = ActiveTransition.ToStateIndex;
        UpdateStateTime(ActiveTransition.FromStateIndex, DeltaSeconds);
        UpdateStateTime(ActiveTransition.ToStateIndex, DeltaSeconds);

        if (const FAnimTransitionDesc* InterruptTransition = FindBestInterruptTransition())
        {
            TArray<FTransform> CurrentBlendedPose;
            if (EvaluateAnimation(CurrentBlendedPose))
            {
                StartTransitionFromSnapshot(*InterruptTransition, CurrentBlendedPose);
                return;
            }
        }

        ActiveTransition.ElapsedTime += DeltaSeconds;
        if (GetTransitionAlpha() >= 1.0f)
        {
            FinishTransition();
        }
        return;
    }

    TargetStateIndex = -1;
    UpdateStateTime(CurrentStateIndex, DeltaSeconds);

    if (const FAnimTransitionDesc* CandidateTransition = FindBestTransitionFromState(CurrentStateIndex))
    {
        StartTransition(*CandidateTransition);
        return;
    }

    if (const FAnimTransitionDesc* AnyTransition = FindBestAnyTransition(CurrentStateIndex))
    {
        StartTransition(*AnyTransition);
    }
}

bool UAnimStateMachineInstance::EvaluateAnimation(TArray<FTransform>& OutLocalPose)
{
    USkeletalMeshComponent* Component = GetSkelMeshComponent();
    const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
    if (!Mesh)
    {
        return false;
    }

    if (!ActiveTransition.bActive)
    {
        return EvaluateStatePose(CurrentStateIndex, Mesh, OutLocalPose);
    }

    TArray<FTransform> SourcePose;
    if (ActiveTransition.bUseSourcePoseSnapshot)
    {
        SourcePose = ActiveTransition.SourcePoseSnapshot;
    }
    else if (!EvaluateStatePose(ActiveTransition.FromStateIndex, Mesh, SourcePose))
    {
        return false;
    }

    TArray<FTransform> TargetPose;
    if (!EvaluateStatePose(ActiveTransition.ToStateIndex, Mesh, TargetPose))
    {
        return false;
    }

    return FAnimationRuntime::BlendLocalPoses(SourcePose, TargetPose, GetTransitionAlpha(), OutLocalPose);
}

FName UAnimStateMachineInstance::GetCurrentStateName() const
{
    const FAnimStateRuntime* State = GetRuntimeState(CurrentStateIndex);
    return State && State->Desc ? State->Desc->Name : FName();
}

FName UAnimStateMachineInstance::GetPreviousStateName() const
{
    const FAnimStateRuntime* State = GetRuntimeState(PreviousStateIndex);
    return State && State->Desc ? State->Desc->Name : FName();
}

FName UAnimStateMachineInstance::GetTargetStateName() const
{
    const int32 StateIndex = ActiveTransition.bActive ? ActiveTransition.ToStateIndex : TargetStateIndex;
    const FAnimStateRuntime* State = GetRuntimeState(StateIndex);
    return State && State->Desc ? State->Desc->Name : FName();
}

float UAnimStateMachineInstance::GetTransitionAlpha() const
{
    if (!ActiveTransition.bActive || ActiveTransition.BlendTime <= AnimationTimeEpsilon)
    {
        return 0.0f;
    }

    return std::clamp(ActiveTransition.ElapsedTime / ActiveTransition.BlendTime, 0.0f, 1.0f);
}

bool UAnimStateMachineInstance::IsTransitioning() const
{
    return ActiveTransition.bActive;
}

void UAnimStateMachineInstance::ResetRuntime()
{
    StateMachineAsset = nullptr;
    Desc = FAnimStateMachineDesc();
    RuntimeStates.clear();
    RuntimeTransitions.clear();
    CurrentStateIndex = -1;
    PreviousStateIndex = -1;
    TargetStateIndex = -1;
    ActiveTransition = FAnimTransitionRuntime();
    MissingConditionWarningKeys.clear();
}

bool UAnimStateMachineInstance::IsValidStateIndex(int32 StateIndex) const
{
    return StateIndex >= 0 && StateIndex < static_cast<int32>(RuntimeStates.size());
}

int32 UAnimStateMachineInstance::FindRuntimeStateIndexByName(const FName& StateName) const
{
    if (!StateName.IsValid())
    {
        return -1;
    }

    for (int32 StateIndex = 0; StateIndex < static_cast<int32>(RuntimeStates.size()); ++StateIndex)
    {
        const FAnimStateRuntime& RuntimeState = RuntimeStates[StateIndex];
        if (RuntimeState.Desc && RuntimeState.Desc->Name == StateName)
        {
            return StateIndex;
        }
    }

    return -1;
}

const UAnimStateMachineInstance::FAnimStateRuntime* UAnimStateMachineInstance::GetRuntimeState(int32 StateIndex) const
{
    return IsValidStateIndex(StateIndex) ? &RuntimeStates[StateIndex] : nullptr;
}

UAnimStateMachineInstance::FAnimStateRuntime* UAnimStateMachineInstance::GetRuntimeState(int32 StateIndex)
{
    return IsValidStateIndex(StateIndex) ? &RuntimeStates[StateIndex] : nullptr;
}

void UAnimStateMachineInstance::UpdateStateTime(int32 StateIndex, float DeltaSeconds)
{
    FAnimStateRuntime* State = GetRuntimeState(StateIndex);
    if (!State || !State->Desc || !State->Sequence)
    {
        return;
    }

    State->PreviousTime = State->CurrentTime;

    const float PlayLength = State->Sequence->GetPlayLength();
    if (PlayLength <= AnimationTimeEpsilon)
    {
        State->CurrentTime = 0.0f;
        return;
    }

    const float PlayRate = State->Desc->PlayRate;
    if (std::fabs(PlayRate) <= AnimationTimeEpsilon)
    {
        return;
    }

    const float NewTime = State->CurrentTime + DeltaSeconds * PlayRate;
    State->CurrentTime = State->Desc->bLooping
        ? WrapStateTime(NewTime, PlayLength)
        : ClampStateTime(NewTime, PlayLength);
}

void UAnimStateMachineInstance::StartTransition(const FAnimTransitionDesc& Transition)
{
    const int32 ToStateIndex = FindRuntimeStateIndexByName(Transition.ToState);
    if (!IsValidStateIndex(CurrentStateIndex) ||
        !IsValidStateIndex(ToStateIndex) ||
        ToStateIndex == CurrentStateIndex)
    {
        return;
    }

    FAnimStateRuntime* TargetState = GetRuntimeState(ToStateIndex);
    if (!TargetState)
    {
        return;
    }

    PreviousStateIndex = CurrentStateIndex;
    TargetStateIndex = ToStateIndex;

    // target state는 transition에 진입하는 순간부터 시간 0.0으로 재생
    TargetState->PreviousTime = 0.0f;
    TargetState->CurrentTime = 0.0f;

    ActiveTransition = FAnimTransitionRuntime();
    ActiveTransition.bActive = true;
    ActiveTransition.FromStateIndex = CurrentStateIndex;
    ActiveTransition.ToStateIndex = ToStateIndex;
    ActiveTransition.ElapsedTime = 0.0f;
    ActiveTransition.BlendTime = Transition.BlendTime;
    ActiveTransition.Priority = Transition.Priority;

    if (ActiveTransition.BlendTime <= AnimationTimeEpsilon)
    {
        FinishTransition();
    }
}

void UAnimStateMachineInstance::StartTransitionFromSnapshot(
    const FAnimTransitionDesc& Transition,
    const TArray<FTransform>& SourcePoseSnapshot)
{
    const int32 ToStateIndex = FindRuntimeStateIndexByName(Transition.ToState);
    if (SourcePoseSnapshot.empty() ||
        !ActiveTransition.bActive ||
        !IsValidStateIndex(ToStateIndex) ||
        ToStateIndex == ActiveTransition.ToStateIndex)
    {
        return;
    }

    FAnimStateRuntime* TargetState = GetRuntimeState(ToStateIndex);
    if (!TargetState)
    {
        return;
    }

    PreviousStateIndex = ActiveTransition.ToStateIndex;
    TargetStateIndex = ToStateIndex;

    // 현재 화면에 보이던 blended pose를 새 transition의 source로 고정
    TargetState->PreviousTime = 0.0f;
    TargetState->CurrentTime = 0.0f;

    const int32 SnapshotSourceStateIndex = ActiveTransition.ToStateIndex;
    ActiveTransition = FAnimTransitionRuntime();
    ActiveTransition.bActive = true;
    ActiveTransition.FromStateIndex = SnapshotSourceStateIndex;
    ActiveTransition.ToStateIndex = ToStateIndex;
    ActiveTransition.ElapsedTime = 0.0f;
    ActiveTransition.BlendTime = Transition.BlendTime;
    ActiveTransition.Priority = Transition.Priority;
    ActiveTransition.bUseSourcePoseSnapshot = true;
    ActiveTransition.SourcePoseSnapshot = SourcePoseSnapshot;

    if (ActiveTransition.BlendTime <= AnimationTimeEpsilon)
    {
        FinishTransition();
    }
}

void UAnimStateMachineInstance::FinishTransition()
{
    const int32 FinishedFromStateIndex = ActiveTransition.FromStateIndex;
    const int32 FinishedToStateIndex = ActiveTransition.ToStateIndex;

    if (IsValidStateIndex(FinishedToStateIndex))
    {
        CurrentStateIndex = FinishedToStateIndex;
        PreviousStateIndex = FinishedFromStateIndex;
    }

    TargetStateIndex = -1;
    ActiveTransition = FAnimTransitionRuntime();
}

bool UAnimStateMachineInstance::EvaluateStatePose(
    int32 StateIndex,
    const USkeletalMesh* Mesh,
    TArray<FTransform>& OutPose) const
{
    const FAnimStateRuntime* State = GetRuntimeState(StateIndex);
    if (!Mesh || !State || !State->Desc || !State->Sequence)
    {
        return false;
    }

    const FAnimExtractContext ExtractContext(
        State->CurrentTime,
        State->Desc->bLooping);
    return State->Sequence->GetAnimationPose(OutPose, Mesh, ExtractContext);
}

bool UAnimStateMachineInstance::IsAnyTransition(const FAnimTransitionDesc& Transition) const
{
    return FAnimStateMachineDesc::IsAnyStateName(Transition.FromState);
}

bool UAnimStateMachineInstance::CanUseTransitionTarget(
    const FAnimTransitionDesc& Transition,
    int32 BlockedStateIndex) const
{
    const int32 ToStateIndex = FindRuntimeStateIndexByName(Transition.ToState);
    return ToStateIndex >= 0 && ToStateIndex != BlockedStateIndex;
}

const FAnimTransitionDesc* UAnimStateMachineInstance::FindBestTransitionFromState(int32 StateIndex) const
{
    const FAnimStateRuntime* State = GetRuntimeState(StateIndex);
    if (!State || !State->Desc)
    {
        return nullptr;
    }

    const FName& StateName = State->Desc->Name;
    const FAnimTransitionDesc* BestTransition = nullptr;
    for (const FAnimTransitionDesc* Transition : RuntimeTransitions)
    {
        if (!Transition || IsAnyTransition(*Transition) || Transition->FromState != StateName)
        {
            continue;
        }

        if (!CanUseTransitionTarget(*Transition, StateIndex))
        {
            continue;
        }

        if (!EvaluateTransitionCondition(*Transition))
        {
            continue;
        }

        // 같은 priority면 JSON 순서 상 먼저 찾은 후보를 사용
        BestTransition = ChooseHigherPriorityTransition(BestTransition, Transition);
    }

    return BestTransition;
}

const FAnimTransitionDesc* UAnimStateMachineInstance::FindBestAnyTransition(int32 BlockedStateIndex) const
{
    const FAnimTransitionDesc* BestTransition = nullptr;
    for (const FAnimTransitionDesc* Transition : RuntimeTransitions)
    {
        if (!Transition || !IsAnyTransition(*Transition))
        {
            continue;
        }

        if (!CanUseTransitionTarget(*Transition, BlockedStateIndex))
        {
            continue;
        }

        if (!EvaluateTransitionCondition(*Transition))
        {
            continue;
        }

        // 같은 priority면 JSON 순서 상 먼저 찾은 후보를 사용
        BestTransition = ChooseHigherPriorityTransition(BestTransition, Transition);
    }

    return BestTransition;
}

const FAnimTransitionDesc* UAnimStateMachineInstance::FindBestInterruptTransition() const
{
    if (!ActiveTransition.bActive || !IsValidStateIndex(ActiveTransition.ToStateIndex))
    {
        return nullptr;
    }

    const int32 BlockedStateIndex = ActiveTransition.ToStateIndex;
    const FAnimStateRuntime* TargetState = GetRuntimeState(ActiveTransition.ToStateIndex);
    const FName TargetStateName = TargetState && TargetState->Desc ? TargetState->Desc->Name : FName();

    const FAnimTransitionDesc* BestTransition = nullptr;
    for (const FAnimTransitionDesc* Transition : RuntimeTransitions)
    {
        if (!Transition)
        {
            continue;
        }

        const bool bAnyTransition = IsAnyTransition(*Transition);
        const bool bTargetOutgoingTransition =
            !bAnyTransition && TargetStateName.IsValid() && Transition->FromState == TargetStateName;
        if (!bAnyTransition && !bTargetOutgoingTransition)
        {
            continue;
        }

        if (Transition->Priority <= ActiveTransition.Priority ||
            !CanUseTransitionTarget(*Transition, BlockedStateIndex) ||
            !EvaluateTransitionCondition(*Transition))
        {
            continue;
        }

        BestTransition = ChooseHigherPriorityTransition(BestTransition, Transition);
    }

    return BestTransition;
}

bool UAnimStateMachineInstance::EvaluateTransitionCondition(const FAnimTransitionDesc& Transition) const
{
    return EvaluateStructuredCondition(Transition.Condition, Transition);
}

bool UAnimStateMachineInstance::EvaluateStructuredCondition(
    const FAnimTransitionConditionDesc& Condition,
    const FAnimTransitionDesc& Transition) const
{
    switch (Condition.Type)
    {
    case EAnimConditionType::None:
        return true;
    case EAnimConditionType::Bool:
    {
        bool CurrentValue = false;
        if (!GetAnimVariableBool(Condition.VariableName, CurrentValue))
        {
            WarnMissingConditionVariableOnce(Transition, Condition);
            return false;
        }

        return CompareBool(CurrentValue, Condition.Operator, Condition.BoolValue);
    }
    case EAnimConditionType::Float:
    {
        float CurrentValue = 0.0f;
        if (!GetAnimVariableFloat(Condition.VariableName, CurrentValue))
        {
            WarnMissingConditionVariableOnce(Transition, Condition);
            return false;
        }

        return CompareFloat(CurrentValue, Condition.Operator, Condition.FloatValue);
    }
    case EAnimConditionType::LuaFunction:
        return false;
    default:
        return false;
    }
}

bool UAnimStateMachineInstance::CompareFloat(float Lhs, EAnimCompareOperator Operator, float Rhs)
{
    switch (Operator)
    {
    case EAnimCompareOperator::Equal:
        return std::fabs(Lhs - Rhs) <= ConditionFloatEpsilon;
    case EAnimCompareOperator::NotEqual:
        return std::fabs(Lhs - Rhs) > ConditionFloatEpsilon;
    case EAnimCompareOperator::Greater:
        return Lhs > Rhs;
    case EAnimCompareOperator::GreaterEqual:
        return Lhs >= Rhs;
    case EAnimCompareOperator::Less:
        return Lhs < Rhs;
    case EAnimCompareOperator::LessEqual:
        return Lhs <= Rhs;
    default:
        return false;
    }
}

bool UAnimStateMachineInstance::CompareBool(bool Lhs, EAnimCompareOperator Operator, bool Rhs)
{
    switch (Operator)
    {
    case EAnimCompareOperator::Equal:
        return Lhs == Rhs;
    case EAnimCompareOperator::NotEqual:
        return Lhs != Rhs;
    default:
        return false;
    }
}

void UAnimStateMachineInstance::WarnMissingConditionVariableOnce(
    const FAnimTransitionDesc& Transition,
    const FAnimTransitionConditionDesc& Condition) const
{
    const FString WarningKey =
        std::to_string(Transition.Id) + "|" +
        ToConditionTypeDebugName(Condition.Type) + "|" +
        Condition.VariableName.ToString();

    if (MissingConditionWarningKeys.find(WarningKey) != MissingConditionWarningKeys.end())
    {
        return;
    }

    MissingConditionWarningKeys.insert(WarningKey);
    UE_LOG_WARNING(
        "[AnimStateMachineInstance] Missing animation variable for transition condition. TransitionId=%d From=%s To=%s Type=%s Variable=%s",
        Transition.Id,
        Transition.FromState.ToString().c_str(),
        Transition.ToState.ToString().c_str(),
        ToConditionTypeDebugName(Condition.Type),
        Condition.VariableName.ToString().c_str());
}
