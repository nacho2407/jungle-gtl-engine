#include "Animation/AnimStateMachineInstance.h"

#include "Animation/AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimStateMachine.h"
#include "Asset/SkeletalMesh.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/AssetPathPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/Stats.h"
#include "Core/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Runtime/Script/ScriptComponent.h"

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
    case EAnimConditionType::Trigger:
        return "Trigger";
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

const FString& UAnimStateMachineInstance::GetStateMachinePath() const
{
    return StateMachinePath;
}

bool UAnimStateMachineInstance::UsesStateMachinePath(const FString& Path) const
{
    const FString NormalizedPath = FAssetPathPolicy::NormalizeAnimStateMachineAssetPath(Path);
    return !NormalizedPath.empty() && StateMachinePath == NormalizedPath;
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
    StateMachinePath = InStateMachine->GetAssetPath();
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
    SCOPE_STAT("Anim.StateMachine.Update");

    if (ActiveTransition.bActive)
    {
        TargetStateIndex = ActiveTransition.ToStateIndex;
        UpdateStateTime(ActiveTransition.FromStateIndex, DeltaSeconds);
        UpdateStateTime(ActiveTransition.ToStateIndex, DeltaSeconds);

        if (const FAnimTransitionDesc* InterruptTransition = FindBestInterruptTransition())
        {
            TArray<FTransform> CurrentBlendedPose;
            USkeletalMeshComponent* Component = GetSkelMeshComponent();
            const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
            if (EvaluateCurrentPose(Mesh, CurrentBlendedPose))
            {
                StartTransitionFromSnapshot(*InterruptTransition, CurrentBlendedPose);
                return;
            }
        }

        const float TransitionAlpha = GetTransitionAlpha();
        if (!ActiveTransition.bUseSourcePoseSnapshot)
        {
            TriggerStateNotifies(
                ActiveTransition.FromStateIndex,
                DeltaSeconds,
                1.0f - TransitionAlpha,
                true,
                false);
        }
        TriggerStateNotifies(
            ActiveTransition.ToStateIndex,
            DeltaSeconds,
            TransitionAlpha,
            false,
            true);

        ActiveTransition.ElapsedTime += DeltaSeconds;
        if (GetTransitionAlpha() >= 1.0f)
        {
            FinishTransition();
        }
        return;
    }

    TargetStateIndex = -1;
    UpdateStateTime(CurrentStateIndex, DeltaSeconds);
    TriggerStateNotifies(CurrentStateIndex, DeltaSeconds, 1.0f, false, false);

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
    SCOPE_STAT("Anim.StateMachine.Evaluate");

    USkeletalMeshComponent* Component = GetSkelMeshComponent();
    const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
    if (!Mesh)
    {
        return false;
    }

    if (!EvaluateCurrentPose(Mesh, OutLocalPose))
    {
        return false;
    }

    ProcessRootMotion(OutLocalPose, ShouldResetRootMotionForCurrentPose());
    return true;
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
    for (int32 StateIndex = 0; StateIndex < static_cast<int32>(RuntimeStates.size()); ++StateIndex)
    {
        ClearStateNotifyStates(StateIndex, true);
    }

    StateMachineAsset = nullptr;
    StateMachinePath.clear();
    Desc = FAnimStateMachineDesc();
    RuntimeStates.clear();
    RuntimeTransitions.clear();
    CurrentStateIndex = -1;
    PreviousStateIndex = -1;
    TargetStateIndex = -1;
    ActiveTransition = FAnimTransitionRuntime();
    MissingConditionWarningKeys.clear();
    LuaConditionWarningKeys.clear();
    ClearRootMotionState();
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

    State->bLoopedThisFrame = false;
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
    if (State->Desc->bLooping)
    {
        State->bLoopedThisFrame = NewTime < 0.0f || NewTime >= PlayLength;
    }

    State->CurrentTime = State->Desc->bLooping
        ? WrapStateTime(NewTime, PlayLength)
        : ClampStateTime(NewTime, PlayLength);
}

void UAnimStateMachineInstance::TriggerStateNotifies(
    int32 StateIndex,
    float DeltaSeconds,
    float TriggerWeight,
    bool bFromTransitionSource,
    bool bFromTransitionTarget)
{
    SCOPE_STAT("Anim.StateMachine.Notify");

    FAnimStateRuntime* State = GetRuntimeState(StateIndex);
    if (!State || !State->Sequence)
    {
        return;
    }

    FAnimationRuntime::TriggerAnimNotifies(
        MakeStateNotifyContext(
            StateIndex,
            DeltaSeconds,
            TriggerWeight,
            bFromTransitionSource,
            bFromTransitionTarget),
        State->ActiveNotifyStates,
        [this](const FAnimNotifyDispatchEvent& NotifyEvent)
        {
            DispatchAnimNotifyEvent(NotifyEvent);
        });
}

void UAnimStateMachineInstance::ClearStateNotifyStates(int32 StateIndex, bool bDispatchEnd)
{
    FAnimStateRuntime* State = GetRuntimeState(StateIndex);
    if (!State)
    {
        return;
    }

    FAnimationRuntime::ClearActiveAnimNotifyStates(
        State->ActiveNotifyStates,
        [this](const FAnimNotifyDispatchEvent& NotifyEvent)
        {
            DispatchAnimNotifyEvent(NotifyEvent);
        },
        bDispatchEnd,
        MakeStateNotifyContext(StateIndex, 0.0f, 0.0f, false, false));
}

void UAnimStateMachineInstance::ClearInactiveNotifyStates(const TSet<int32>& ActiveStateIndices)
{
    for (int32 StateIndex = 0; StateIndex < static_cast<int32>(RuntimeStates.size()); ++StateIndex)
    {
        if (ActiveStateIndices.find(StateIndex) != ActiveStateIndices.end())
        {
            continue;
        }

        ClearStateNotifyStates(StateIndex, true);
    }
}

FAnimNotifyTriggerContext UAnimStateMachineInstance::MakeStateNotifyContext(
    int32 StateIndex,
    float DeltaSeconds,
    float TriggerWeight,
    bool bFromTransitionSource,
    bool bFromTransitionTarget) const
{
    FAnimNotifyTriggerContext Context;

    const FAnimStateRuntime* State = GetRuntimeState(StateIndex);
    if (!State || !State->Desc)
    {
        return Context;
    }

    Context.Sequence = State->Sequence;
    Context.PreviousTime = State->PreviousTime;
    Context.CurrentTime = State->CurrentTime;
    Context.DeltaSeconds = DeltaSeconds;
    Context.bLooping = State->Desc->bLooping;
    Context.bReverse = State->Desc->PlayRate < 0.0f;
    Context.bLooped = State->bLoopedThisFrame;
    Context.TriggerWeight = TriggerWeight;
    Context.TriggerWeightThreshold = NotifyTriggerWeightThreshold;
    Context.SourceStateName = State->Desc->Name;
    Context.SourceAnimationName = State->Desc->Animation.AnimStackName.empty()
        ? (State->Sequence ? State->Sequence->GetFName() : FName())
        : FName(State->Desc->Animation.AnimStackName);
    Context.bFromStateMachine = true;
    Context.bFromTransitionSource = bFromTransitionSource;
    Context.bFromTransitionTarget = bFromTransitionTarget;
    return Context;
}

void UAnimStateMachineInstance::DispatchAnimNotifyEvent(const FAnimNotifyDispatchEvent& NotifyEvent)
{
    if (USkeletalMeshComponent* Component = GetSkelMeshComponent())
    {
        Component->HandleAnimNotify(NotifyEvent);
    }
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

    ConsumeTransitionTriggerIfNeeded(Transition);
    ClearRootMotionState();

    // target state는 transition에 진입하는 순간부터 시간 0.0으로 재생
    ClearStateNotifyStates(ToStateIndex, true);
    TargetState->PreviousTime = 0.0f;
    TargetState->CurrentTime = 0.0f;
    TargetState->bLoopedThisFrame = false;

    ActiveTransition = FAnimTransitionRuntime();
    ActiveTransition.bActive = true;
    ActiveTransition.FromStateIndex = CurrentStateIndex;
    ActiveTransition.ToStateIndex = ToStateIndex;
    ActiveTransition.ElapsedTime = 0.0f;
    ActiveTransition.BlendTime = Transition.BlendTime;
    ActiveTransition.Priority = Transition.Priority;
    ActiveTransition.bCanInterrupt = Transition.bCanInterrupt;
    ActiveTransition.bCanBeInterrupted = Transition.bCanBeInterrupted;

    if (ActiveTransition.BlendTime <= AnimationTimeEpsilon)
    {
        FinishTransition();
        return;
    }

    TSet<int32> ActiveStateIndices;
    ActiveStateIndices.insert(ActiveTransition.FromStateIndex);
    ActiveStateIndices.insert(ActiveTransition.ToStateIndex);
    ClearInactiveNotifyStates(ActiveStateIndices);
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

    ConsumeTransitionTriggerIfNeeded(Transition);
    ClearRootMotionState();

    // 현재 화면에 보이던 blended pose를 새 transition의 source로 고정
    ClearStateNotifyStates(ToStateIndex, true);
    TargetState->PreviousTime = 0.0f;
    TargetState->CurrentTime = 0.0f;
    TargetState->bLoopedThisFrame = false;

    const int32 SnapshotSourceStateIndex = ActiveTransition.ToStateIndex;
    ActiveTransition = FAnimTransitionRuntime();
    ActiveTransition.bActive = true;
    ActiveTransition.FromStateIndex = SnapshotSourceStateIndex;
    ActiveTransition.ToStateIndex = ToStateIndex;
    ActiveTransition.ElapsedTime = 0.0f;
    ActiveTransition.BlendTime = Transition.BlendTime;
    ActiveTransition.Priority = Transition.Priority;
    ActiveTransition.bCanInterrupt = Transition.bCanInterrupt;
    ActiveTransition.bCanBeInterrupted = Transition.bCanBeInterrupted;
    ActiveTransition.bUseSourcePoseSnapshot = true;
    ActiveTransition.SourcePoseSnapshot = SourcePoseSnapshot;

    if (ActiveTransition.BlendTime <= AnimationTimeEpsilon)
    {
        FinishTransition();
        return;
    }

    TSet<int32> ActiveStateIndices;
    ActiveStateIndices.insert(ActiveTransition.ToStateIndex);
    ClearInactiveNotifyStates(ActiveStateIndices);
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
    ClearRootMotionState();

    TSet<int32> ActiveStateIndices;
    if (IsValidStateIndex(CurrentStateIndex))
    {
        ActiveStateIndices.insert(CurrentStateIndex);
    }
    ClearInactiveNotifyStates(ActiveStateIndices);
}

bool UAnimStateMachineInstance::EvaluateCurrentPose(const USkeletalMesh* Mesh, TArray<FTransform>& OutPose) const
{
    if (!Mesh)
    {
        return false;
    }

    if (!ActiveTransition.bActive)
    {
        return EvaluateStatePose(CurrentStateIndex, Mesh, OutPose);
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

    return FAnimationRuntime::BlendLocalPoses(SourcePose, TargetPose, GetTransitionAlpha(), OutPose);
}

bool UAnimStateMachineInstance::ShouldResetRootMotionForCurrentPose() const
{
    if (!ActiveTransition.bActive)
    {
        const FAnimStateRuntime* State = GetRuntimeState(CurrentStateIndex);
        return State && State->bLoopedThisFrame;
    }

    const FAnimStateRuntime* SourceState = ActiveTransition.bUseSourcePoseSnapshot
        ? nullptr
        : GetRuntimeState(ActiveTransition.FromStateIndex);
    const FAnimStateRuntime* TargetState = GetRuntimeState(ActiveTransition.ToStateIndex);
    return (SourceState && SourceState->bLoopedThisFrame) || (TargetState && TargetState->bLoopedThisFrame);
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
    SCOPE_STAT("Anim.StateMachine.Interrupt");

    if (!ActiveTransition.bActive ||
        !ActiveTransition.bCanBeInterrupted ||
        !IsValidStateIndex(ActiveTransition.ToStateIndex))
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

        if (!Transition->bCanInterrupt ||
            Transition->Priority <= ActiveTransition.Priority ||
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
    SCOPE_STAT("Anim.StateMachine.Condition");

    return EvaluateStructuredCondition(Transition.Condition, Transition);
}

void UAnimStateMachineInstance::ConsumeTransitionTriggerIfNeeded(const FAnimTransitionDesc& Transition)
{
    if (Transition.Condition.Type == EAnimConditionType::Trigger)
    {
        ConsumeAnimTrigger(Transition.Condition.VariableName);
    }
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
    case EAnimConditionType::Trigger:
        return IsAnimTriggerSet(Condition.VariableName);
    case EAnimConditionType::LuaFunction:
        return EvaluateLuaCondition(Transition, Condition);
    default:
        return false;
    }
}

bool UAnimStateMachineInstance::EvaluateLuaCondition(
    const FAnimTransitionDesc& Transition,
    const FAnimTransitionConditionDesc& Condition) const
{
    const FName& FunctionName = Condition.LuaFunctionName;
    if (!FunctionName.IsValid())
    {
        WarnLuaConditionOnce(Transition, FunctionName, "function name is empty");
        return false;
    }

    USkeletalMeshComponent* Component = GetSkelMeshComponent();
    AActor* OwnerActor = Component ? Component->GetOwner() : nullptr;
    if (!OwnerActor)
    {
        WarnLuaConditionOnce(Transition, FunctionName, "owner actor was not found");
        return false;
    }

    const FString FunctionNameString = FunctionName.ToString();
    TArray<UScriptComponent*> MatchingScriptComponents;
    bool bHasScriptComponent = false;
    bool bHasUnloadedScriptComponent = false;

    for (UActorComponent* ActorComponent : OwnerActor->GetComponents())
    {
        UScriptComponent* ScriptComponent = Cast<UScriptComponent>(ActorComponent);
        if (!ScriptComponent)
        {
            continue;
        }

        bHasScriptComponent = true;
        if (!ScriptComponent->IsScriptLoaded())
        {
            bHasUnloadedScriptComponent = true;
            continue;
        }

        if (ScriptComponent->HasScriptFunction(FunctionNameString))
        {
            MatchingScriptComponents.push_back(ScriptComponent);
        }
    }

    if (!bHasScriptComponent)
    {
        WarnLuaConditionOnce(Transition, FunctionName, "owner actor has no script component");
        return false;
    }

    if (MatchingScriptComponents.empty())
    {
        const FString Reason = bHasUnloadedScriptComponent
            ? "function was not found in loaded script components; at least one script component is not loaded"
            : "function was not found in script components";
        WarnLuaConditionOnce(Transition, FunctionName, Reason);
        return false;
    }

    if (MatchingScriptComponents.size() > 1)
    {
        WarnLuaConditionOnce(
            Transition,
            FunctionName,
            "multiple script components define the function; using first match");
    }

    bool bLuaResult = false;
    FString FailureReason;
    if (!MatchingScriptComponents.front()->CallBoolFunction(FunctionNameString, bLuaResult, &FailureReason))
    {
        WarnLuaConditionOnce(
            Transition,
            FunctionName,
            FailureReason.empty() ? "function call failed" : FailureReason);
        return false;
    }

    return bLuaResult;
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

void UAnimStateMachineInstance::WarnLuaConditionOnce(
    const FAnimTransitionDesc& Transition,
    const FName& FunctionName,
    const FString& Reason) const
{
    const FString WarningKey =
        std::to_string(Transition.Id) + "|LuaFunction|" +
        FunctionName.ToString() + "|" +
        Reason;

    if (LuaConditionWarningKeys.find(WarningKey) != LuaConditionWarningKeys.end())
    {
        return;
    }

    LuaConditionWarningKeys.insert(WarningKey);
    UE_LOG_WARNING(
        "[AnimStateMachineInstance] Lua condition warning. TransitionId=%d From=%s To=%s Function=%s Reason=%s",
        Transition.Id,
        Transition.FromState.ToString().c_str(),
        Transition.ToState.ToString().c_str(),
        FunctionName.ToString().c_str(),
        Reason.c_str());
}
