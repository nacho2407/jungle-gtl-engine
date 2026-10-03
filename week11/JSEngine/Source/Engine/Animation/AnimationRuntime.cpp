#include "Animation/AnimationRuntime.h"

#include "Animation/AnimSequence.h"
#include "Asset/SkeletalMesh.h"

#include <algorithm>
#include <utility>

namespace
{
constexpr float NotifyTimeEpsilon = 1.0e-6f;

bool IsInstantNotify(const FAnimNotifyEvent& Notify)
{
    return Notify.Duration <= 0.0f;
}

bool IsNotifyTimeInRange(float NotifyTime, float RangeStart, float RangeEnd, bool bReverse)
{
    if (!bReverse)
    {
        return NotifyTime > RangeStart && NotifyTime <= RangeEnd;
    }

    return NotifyTime >= RangeEnd && NotifyTime < RangeStart;
}

bool IsTimeInsideNotifyState(float Time, const FAnimNotifyEvent& Notify)
{
    const float NotifyStart = Notify.TriggerTime;
    const float NotifyEnd = Notify.TriggerTime + Notify.Duration;
    return Time >= NotifyStart && Time <= NotifyEnd;
}

bool CanTriggerNotifyAtWeight(const FAnimNotifyTriggerContext& Context)
{
    return Context.TriggerWeight >= Context.TriggerWeightThreshold;
}

FActiveAnimNotifyState* FindActiveNotifyState(
    TArray<FActiveAnimNotifyState>& ActiveNotifyStates,
    int32 NotifyIndex)
{
    for (FActiveAnimNotifyState& ActiveState : ActiveNotifyStates)
    {
        if (ActiveState.NotifyIndex == NotifyIndex)
        {
            return &ActiveState;
        }
    }

    return nullptr;
}

FAnimNotifyDispatchEvent MakeNotifyDispatchEvent(
    const FAnimNotifyTriggerContext& Context,
    const FAnimNotifyEvent& Notify,
    EAnimNotifyPhase Phase,
    const FName& SourceStateName,
    float TriggerWeight)
{
    FAnimNotifyDispatchEvent Event;
    Event.Notify = Notify;
    Event.Phase = Phase;
    Event.SourceStateName = SourceStateName.IsValid() ? SourceStateName : Context.SourceStateName;
    Event.SourceAnimationName = Context.SourceAnimationName;
    Event.TriggerWeight = TriggerWeight;
    Event.CurrentTime = Context.CurrentTime;
    Event.bFromStateMachine = Context.bFromStateMachine;
    Event.bFromTransitionSource = Context.bFromTransitionSource;
    Event.bFromTransitionTarget = Context.bFromTransitionTarget;
    return Event;
}

void DispatchNotifyEvent(
    const FAnimNotifyTriggerContext& Context,
    const FAnimNotifyEvent& Notify,
    EAnimNotifyPhase Phase,
    const FAnimationRuntime::FAnimNotifyDispatchFunction& DispatchFunc)
{
    if (!DispatchFunc)
    {
        return;
    }

    DispatchFunc(MakeNotifyDispatchEvent(
        Context,
        Notify,
        Phase,
        Context.SourceStateName,
        Context.TriggerWeight));
}

void DispatchActiveNotifyEnd(
    const FAnimNotifyTriggerContext& Context,
    const FActiveAnimNotifyState& ActiveState,
    const FAnimationRuntime::FAnimNotifyDispatchFunction& DispatchFunc)
{
    if (!DispatchFunc)
    {
        return;
    }

    DispatchFunc(MakeNotifyDispatchEvent(
        Context,
        ActiveState.Notify,
        EAnimNotifyPhase::End,
        ActiveState.SourceStateName,
        Context.TriggerWeight));
}

void AddActiveNotifyState(
    TArray<FActiveAnimNotifyState>& ActiveNotifyStates,
    int32 NotifyIndex,
    const FAnimNotifyEvent& Notify,
    const FAnimNotifyTriggerContext& Context)
{
    if (FindActiveNotifyState(ActiveNotifyStates, NotifyIndex))
    {
        return;
    }

    FActiveAnimNotifyState ActiveState;
    ActiveState.NotifyIndex = NotifyIndex;
    ActiveState.Notify = Notify;
    ActiveState.SourceStateName = Context.SourceStateName;
    ActiveState.LastTriggerWeight = Context.TriggerWeight;
    ActiveNotifyStates.push_back(ActiveState);
}

void RemoveActiveNotifyState(
    TArray<FActiveAnimNotifyState>& ActiveNotifyStates,
    int32 NotifyIndex,
    bool bDispatchEnd,
    const FAnimNotifyTriggerContext& Context,
    const FAnimationRuntime::FAnimNotifyDispatchFunction& DispatchFunc)
{
    for (auto It = ActiveNotifyStates.begin(); It != ActiveNotifyStates.end(); ++It)
    {
        if (It->NotifyIndex != NotifyIndex)
        {
            continue;
        }

        const FActiveAnimNotifyState ActiveState = *It;
        ActiveNotifyStates.erase(It);

        if (bDispatchEnd)
        {
            DispatchActiveNotifyEnd(Context, ActiveState, DispatchFunc);
        }

        return;
    }
}
} // namespace

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

void FAnimationRuntime::TriggerAnimNotifies(
    const FAnimNotifyTriggerContext& Context,
    TArray<FActiveAnimNotifyState>& InOutActiveNotifyStates,
    const FAnimNotifyDispatchFunction& DispatchFunc)
{
    if (!Context.Sequence)
    {
        return;
    }

    const TArray<FAnimNotifyEvent>& Notifies = Context.Sequence->GetNotifies();
    if (Notifies.empty())
    {
        return;
    }

    const float PlayLength = Context.Sequence->GetPlayLength();
    if (PlayLength <= NotifyTimeEpsilon)
    {
        return;
    }

    if (!CanTriggerNotifyAtWeight(Context))
    {
        ClearActiveAnimNotifyStates(InOutActiveNotifyStates, DispatchFunc, true, Context);
        return;
    }

    const bool bWrapped = Context.bLooping && Context.bLooped;
    const bool bReverseRange = bWrapped ? Context.bReverse : Context.CurrentTime < Context.PreviousTime;

    if (bWrapped)
    {
        ClearActiveAnimNotifyStates(InOutActiveNotifyStates, DispatchFunc, true, Context);
    }

	// instant notify
    for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Notifies.size()); ++NotifyIndex)
    {
        const FAnimNotifyEvent& Notify = Notifies[NotifyIndex];
        if (!IsInstantNotify(Notify))
        {
            continue;
        }

        bool bShouldDispatch = false;
        if (bWrapped && !bReverseRange)
        {
            bShouldDispatch =
                IsNotifyTimeInRange(Notify.TriggerTime, Context.PreviousTime, PlayLength, false)
                || IsNotifyTimeInRange(Notify.TriggerTime, 0.0f, Context.CurrentTime, false);
        }
        else if (bWrapped)
        {
            bShouldDispatch =
                IsNotifyTimeInRange(Notify.TriggerTime, Context.PreviousTime, 0.0f, true)
                || IsNotifyTimeInRange(Notify.TriggerTime, PlayLength, Context.CurrentTime, true);
        }
        else
        {
            bShouldDispatch = IsNotifyTimeInRange(
                Notify.TriggerTime,
                Context.PreviousTime,
                Context.CurrentTime,
                bReverseRange);
        }

        if (bShouldDispatch)
        {
            DispatchNotifyEvent(Context, Notify, EAnimNotifyPhase::Instant, DispatchFunc);
        }
    }

    if (bWrapped)
    {
        /**
		 * @TODO loop 경계에서 duration notify 처리는 후속 작업으로 남겨둠
		 */
        return;
    }

	// duration notify
    for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Notifies.size()); ++NotifyIndex)
    {
        const FAnimNotifyEvent& Notify = Notifies[NotifyIndex];
        if (IsInstantNotify(Notify))
        {
            continue;
        }

        const bool bPrevInside = IsTimeInsideNotifyState(Context.PreviousTime, Notify);
        const bool bCurrInside = IsTimeInsideNotifyState(Context.CurrentTime, Notify);
        FActiveAnimNotifyState* ActiveState = FindActiveNotifyState(InOutActiveNotifyStates, NotifyIndex);
        const bool bWasActive = ActiveState != nullptr;

        if (!bWasActive && bCurrInside)
        {
            AddActiveNotifyState(InOutActiveNotifyStates, NotifyIndex, Notify, Context);
            DispatchNotifyEvent(Context, Notify, EAnimNotifyPhase::Begin, DispatchFunc);
            ActiveState = FindActiveNotifyState(InOutActiveNotifyStates, NotifyIndex);
        }

        if (bCurrInside && ActiveState)
        {
            ActiveState->LastTriggerWeight = Context.TriggerWeight;
            DispatchNotifyEvent(Context, Notify, EAnimNotifyPhase::Tick, DispatchFunc);
        }

        if ((bWasActive || bPrevInside) && !bCurrInside)
        {
            RemoveActiveNotifyState(InOutActiveNotifyStates, NotifyIndex, true, Context, DispatchFunc);
        }
    }
}

void FAnimationRuntime::ClearActiveAnimNotifyStates(
    TArray<FActiveAnimNotifyState>& InOutActiveNotifyStates,
    const FAnimNotifyDispatchFunction& DispatchFunc,
    bool bDispatchEnd,
    const FAnimNotifyTriggerContext& Context)
{
    if (!bDispatchEnd)
    {
        InOutActiveNotifyStates.clear();
        return;
    }

    const TArray<FActiveAnimNotifyState> StatesToEnd = InOutActiveNotifyStates;
    InOutActiveNotifyStates.clear();
    for (const FActiveAnimNotifyState& ActiveState : StatesToEnd)
    {
        DispatchActiveNotifyEnd(Context, ActiveState, DispatchFunc);
    }
}

bool FAnimationRuntime::HasMatchingBoneCount(const USkeletalMesh* Mesh, const TArray<FTransform>& LocalPose)
{
    return Mesh && Mesh->GetBones().size() == LocalPose.size();
}
