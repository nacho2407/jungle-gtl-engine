#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimStateMachineTypes.h"
#include "Animation/AnimationTypes.h"
#include "Core/Containers/Set.h"

class UAnimSequenceBase;
class UAnimStateMachine;
class USkeletalMesh;

class UAnimStateMachineInstance : public UAnimInstance
{
public:
    DECLARE_CLASS(UAnimStateMachineInstance, UAnimInstance)

    UAnimStateMachineInstance() = default;
    ~UAnimStateMachineInstance() override = default;

    bool SetStateMachine(UAnimStateMachine* InStateMachine);
    bool LoadStateMachine(const FString& Path);

    void NativeInitializeAnimation() override;
    void NativeUninitializeAnimation() override;
    void NativeUpdateAnimation(float DeltaSeconds) override;
    bool EvaluateAnimation(TArray<FTransform>& OutLocalPose) override;

    FName GetCurrentStateName() const;
    FName GetPreviousStateName() const;
    FName GetTargetStateName() const;
    float GetTransitionAlpha() const;
    bool IsTransitioning() const;

private:
    struct FAnimStateRuntime
    {
        const FAnimStateDesc* Desc = nullptr;
        UAnimSequenceBase* Sequence = nullptr;
        float PreviousTime = 0.0f;
        float CurrentTime = 0.0f;
        bool bLoopedThisFrame = false;
        TArray<FActiveAnimNotifyState> ActiveNotifyStates;
    };

    struct FAnimTransitionRuntime
    {
        bool bActive = false;
        int32 FromStateIndex = -1;
        int32 ToStateIndex = -1;
        float ElapsedTime = 0.0f;
        float BlendTime = 0.0f;
        int32 Priority = 0;
        bool bUseSourcePoseSnapshot = false;
        TArray<FTransform> SourcePoseSnapshot;
    };

    void ResetRuntime();
    bool IsValidStateIndex(int32 StateIndex) const;
    int32 FindRuntimeStateIndexByName(const FName& StateName) const;
    const FAnimStateRuntime* GetRuntimeState(int32 StateIndex) const;
    FAnimStateRuntime* GetRuntimeState(int32 StateIndex);
    void UpdateStateTime(int32 StateIndex, float DeltaSeconds);
    void TriggerStateNotifies(
        int32 StateIndex,
        float DeltaSeconds,
        float TriggerWeight,
        bool bFromTransitionSource,
        bool bFromTransitionTarget);
    void ClearStateNotifyStates(int32 StateIndex, bool bDispatchEnd);
    void ClearInactiveNotifyStates(const TSet<int32>& ActiveStateIndices);
    FAnimNotifyTriggerContext MakeStateNotifyContext(
        int32 StateIndex,
        float DeltaSeconds,
        float TriggerWeight,
        bool bFromTransitionSource,
        bool bFromTransitionTarget) const;
    void DispatchAnimNotifyEvent(const FAnimNotifyDispatchEvent& NotifyEvent);
    void StartTransition(const FAnimTransitionDesc& Transition);
    void StartTransitionFromSnapshot(
        const FAnimTransitionDesc& Transition,
        const TArray<FTransform>& SourcePoseSnapshot);
    void FinishTransition();
    bool EvaluateStatePose(int32 StateIndex, const USkeletalMesh* Mesh, TArray<FTransform>& OutPose) const;
    bool IsAnyTransition(const FAnimTransitionDesc& Transition) const;
    bool CanUseTransitionTarget(const FAnimTransitionDesc& Transition, int32 BlockedStateIndex) const;
    const FAnimTransitionDesc* FindBestTransitionFromState(int32 StateIndex) const;
    const FAnimTransitionDesc* FindBestAnyTransition(int32 BlockedStateIndex) const;
    const FAnimTransitionDesc* FindBestInterruptTransition() const;
    bool EvaluateTransitionCondition(const FAnimTransitionDesc& Transition) const;
    bool EvaluateStructuredCondition(
        const FAnimTransitionConditionDesc& Condition,
        const FAnimTransitionDesc& Transition) const;
    bool EvaluateLuaCondition(
        const FAnimTransitionDesc& Transition,
        const FAnimTransitionConditionDesc& Condition) const;
    static bool CompareFloat(float Lhs, EAnimCompareOperator Operator, float Rhs);
    static bool CompareBool(bool Lhs, EAnimCompareOperator Operator, bool Rhs);
    void WarnMissingConditionVariableOnce(
        const FAnimTransitionDesc& Transition,
        const FAnimTransitionConditionDesc& Condition) const;
    void WarnLuaConditionOnce(
        const FAnimTransitionDesc& Transition,
        const FName& FunctionName,
        const FString& Reason) const;

    UAnimStateMachine* StateMachineAsset = nullptr;
    FAnimStateMachineDesc Desc;
    TArray<FAnimStateRuntime> RuntimeStates;
    TArray<const FAnimTransitionDesc*> RuntimeTransitions;

    int32 CurrentStateIndex = -1;
    int32 PreviousStateIndex = -1;
    int32 TargetStateIndex = -1;
    FAnimTransitionRuntime ActiveTransition;

    float NotifyTriggerWeightThreshold = 0.5f;
    mutable TSet<FString> MissingConditionWarningKeys;
    mutable TSet<FString> LuaConditionWarningKeys;
};
