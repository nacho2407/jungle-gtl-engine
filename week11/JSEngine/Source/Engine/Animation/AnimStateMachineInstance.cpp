#include "Animation/AnimStateMachineInstance.h"

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
    TargetStateIndex = -1;

    FAnimStateRuntime* CurrentState = GetRuntimeState(CurrentStateIndex);
    if (!CurrentState || !CurrentState->Desc || !CurrentState->Sequence)
    {
        return;
    }

    CurrentState->PreviousTime = CurrentState->CurrentTime;

    const float PlayLength = CurrentState->Sequence->GetPlayLength();
    if (PlayLength <= AnimationTimeEpsilon)
    {
        CurrentState->CurrentTime = 0.0f;
    }
    else
    {
        const float PlayRate = CurrentState->Desc->PlayRate;
        if (std::fabs(PlayRate) > AnimationTimeEpsilon)
        {
            const float NewTime = CurrentState->CurrentTime + DeltaSeconds * PlayRate;
            // 지금 단계에서는 transition이 없으므로 current state 하나의 시간만 sequence 정책에 맞춰 진행
            CurrentState->CurrentTime = CurrentState->Desc->bLooping
                ? WrapStateTime(NewTime, PlayLength)
                : ClampStateTime(NewTime, PlayLength);
        }
    }

    const FAnimTransitionDesc* CandidateTransition = FindBestTransitionFromState(CurrentStateIndex);
    TargetStateIndex = CandidateTransition
        ? FindRuntimeStateIndexByName(CandidateTransition->ToState)
        : -1;
}

bool UAnimStateMachineInstance::EvaluateAnimation(TArray<FTransform>& OutLocalPose)
{
    USkeletalMeshComponent* Component = GetSkelMeshComponent();
    const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;
    if (!Mesh)
    {
        return false;
    }

    const FAnimStateRuntime* CurrentState = GetRuntimeState(CurrentStateIndex);
    if (!CurrentState || !CurrentState->Desc || !CurrentState->Sequence)
    {
        return false;
    }

    const FAnimExtractContext ExtractContext(
        CurrentState->CurrentTime,
        CurrentState->Desc->bLooping);
    return CurrentState->Sequence->GetAnimationPose(OutLocalPose, Mesh, ExtractContext);
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
        if (!Transition || Transition->FromState != StateName)
        {
            continue;
        }

        if (FindRuntimeStateIndexByName(Transition->ToState) < 0)
        {
            continue;
        }

        if (!EvaluateTransitionCondition(*Transition))
        {
            continue;
        }

        // 같은 priority면 JSON 순서 상 먼저 찾은 후보를 사용
        if (!BestTransition || Transition->Priority > BestTransition->Priority)
        {
            BestTransition = Transition;
        }
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
