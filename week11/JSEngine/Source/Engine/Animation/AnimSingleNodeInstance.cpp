#include "Animation/AnimSingleNodeInstance.h"

#include "Animation/AnimSequence.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/Stats.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

DEFINE_CLASS(UAnimSingleNodeInstance, UAnimInstance)
REGISTER_FACTORY(UAnimSingleNodeInstance)

namespace
{
constexpr float AnimationTimeEpsilon = 1.0e-6f;

float ClampAnimationTime(float Time, float PlayLength)
{
    if (PlayLength <= 0.0f)
    {
        return 0.0f;
    }

    return std::clamp(Time, 0.0f, PlayLength);
}

float WrapAnimationTime(float Time, float PlayLength)
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

float NormalizeAnimationTime(float Time, float PlayLength, bool bLooping)
{
    return bLooping ? WrapAnimationTime(Time, PlayLength) : ClampAnimationTime(Time, PlayLength);
}

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
} // namespace

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimationAsset* NewAsset)
{
    if (CurrentAsset == NewAsset)
    {
        if (!NewAsset)
        {
            ClearActiveNotifyStates(true);
        }
        return;
    }

    ClearActiveNotifyStates(true);

    CurrentAsset = NewAsset;
    PreviousTime = 0.0f;
    CurrentTime = 0.0f;
    bReachedEndThisFrame = false;
    bLoopedThisFrame = false;

    if (!CurrentAsset)
    {
        bPlaying = false;
        bPaused = false;
    }
}

UAnimationAsset* UAnimSingleNodeInstance::GetAnimationAsset() const
{
    return CurrentAsset;
}

void UAnimSingleNodeInstance::Play()
{
    if (!CurrentAsset)
    {
        UE_LOG_WARNING("[AnimSingleNodeInstance] Play called without animation asset.");
        bPlaying = false;
        bPaused = false;
        return;
    }

    bPlaying = true;
    bPaused = false;
}

void UAnimSingleNodeInstance::Pause()
{
    if (bPlaying)
    {
        bPaused = true;
    }
}

void UAnimSingleNodeInstance::Stop()
{
    ClearActiveNotifyStates(true);

    bPlaying = false;
    bPaused = false;
    PreviousTime = 0.0f;
    CurrentTime = 0.0f;
    bReachedEndThisFrame = false;
    bLoopedThisFrame = false;
}

void UAnimSingleNodeInstance::SetPosition(float InTimeSeconds, bool bFireNotifies)
{
    ClearActiveNotifyStates(bFireNotifies);

    PreviousTime = CurrentTime;
    CurrentTime = NormalizeAnimationTime(InTimeSeconds, GetPlayLength(), bLooping);
    bReachedEndThisFrame = false;
    bLoopedThisFrame = false;

    if (bFireNotifies)
    {
        TriggerAnimNotifies();
    }
}

float UAnimSingleNodeInstance::GetPosition() const
{
    return CurrentTime;
}

float UAnimSingleNodeInstance::GetPlayLength() const
{
    return CurrentAsset ? CurrentAsset->GetPlayLength() : 0.0f;
}

void UAnimSingleNodeInstance::SetPlayRate(float InPlayRate)
{
    PlayRate = InPlayRate;
    bReversePlay = PlayRate < 0.0f;
}

float UAnimSingleNodeInstance::GetPlayRate() const
{
    return PlayRate;
}

void UAnimSingleNodeInstance::SetReversePlay(bool bInReversePlay)
{
    float PlayRateMagnitude = std::fabs(PlayRate);
    if (PlayRateMagnitude <= AnimationTimeEpsilon)
    {
        PlayRateMagnitude = 1.0f;
    }

    PlayRate = bInReversePlay ? -PlayRateMagnitude : PlayRateMagnitude;
    bReversePlay = bInReversePlay;
}

bool UAnimSingleNodeInstance::IsReversePlay() const
{
    return bReversePlay;
}

void UAnimSingleNodeInstance::SetLooping(bool bInLooping)
{
    bLooping = bInLooping;
}

bool UAnimSingleNodeInstance::IsLooping() const
{
    return bLooping;
}

bool UAnimSingleNodeInstance::IsPlaying() const
{
    return bPlaying;
}

bool UAnimSingleNodeInstance::IsPaused() const
{
    return bPaused;
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    SCOPE_STAT("Anim.Update");

    if (!bPlaying || bPaused)
    {
        bReachedEndThisFrame = false;
        bLoopedThisFrame = false;
        return;
    }

    AdvanceTime(DeltaSeconds);
    TriggerAnimNotifies();

    if (bReachedEndThisFrame && !bLooping)
    {
        ClearActiveNotifyStates(true);
    }
}

void UAnimSingleNodeInstance::AdvanceTime(float DeltaSeconds)
{
    SCOPE_STAT("Anim.AdvanceTime");

    bReachedEndThisFrame = false;
    bLoopedThisFrame = false;
    PreviousTime = CurrentTime;

    const float PlayLength = GetPlayLength();
    if (PlayLength <= AnimationTimeEpsilon)
    {
        CurrentTime = 0.0f;
        bPlaying = false;
        bPaused = false;
        bReachedEndThisFrame = true;
        return;
    }

    if (std::fabs(PlayRate) <= AnimationTimeEpsilon)
    {
        return;
    }

    const float NewTime = CurrentTime + DeltaSeconds * PlayRate;

    if (bLooping)
    {
        bLoopedThisFrame = NewTime < 0.0f || NewTime >= PlayLength;
        CurrentTime = WrapAnimationTime(NewTime, PlayLength);
        return;
    }

    if (NewTime >= PlayLength)
    {
        CurrentTime = PlayLength;
        bReachedEndThisFrame = true;
        bPlaying = false;
        bPaused = false;
        return;
    }

    if (NewTime <= 0.0f && PlayRate < 0.0f)
    {
        CurrentTime = 0.0f;
        bReachedEndThisFrame = true;
        bPlaying = false;
        bPaused = false;
        return;
    }

    CurrentTime = ClampAnimationTime(NewTime, PlayLength);
}

void UAnimSingleNodeInstance::TriggerAnimNotifies()
{
    SCOPE_STAT("Anim.Notify");

    UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(CurrentAsset);
    if (!Sequence)
    {
        return;
    }

    const TArray<FAnimNotifyEvent>& Notifies = Sequence->GetNotifies();
    if (Notifies.empty())
    {
        return;
    }

    const float PlayLength = GetPlayLength();
    if (PlayLength <= AnimationTimeEpsilon)
    {
        return;
    }

    const bool bWrapped = bLoopedThisFrame;
    const bool bReverseRange = bWrapped ? PlayRate < 0.0f : CurrentTime < PreviousTime;

    if (bWrapped)
    {
        ClearActiveNotifyStates(true);
    }

	// instant notify loop
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
                IsNotifyTimeInRange(Notify.TriggerTime, PreviousTime, PlayLength, false)
                || IsNotifyTimeInRange(Notify.TriggerTime, 0.0f, CurrentTime, false);
        }
        else if (bWrapped)
        {
            bShouldDispatch =
                IsNotifyTimeInRange(Notify.TriggerTime, PreviousTime, 0.0f, true)
                || IsNotifyTimeInRange(Notify.TriggerTime, PlayLength, CurrentTime, true);
        }
        else
        {
            bShouldDispatch = IsNotifyTimeInRange(Notify.TriggerTime, PreviousTime, CurrentTime, bReverseRange);
        }

        if (bShouldDispatch)
        {
            DispatchAnimNotify(Notify, EAnimNotifyPhase::Instant);
        }
    }

    if (bWrapped)
    {
        /**
		 * @TODO Milestone 2에서는 duration notify의 loop wrap 재진입 순서를 완전 보장하지 않음.
		 *       duration notify의 edge case 처리는 별도의 subsystem이 필요할 정도로 경우의 수가 복잡하여
		 *       우선 급한 작업들 우선 처리하기 위해 현재는 건너뜀.
		 * 
		 *       Milestone 3+에서 loop segment split과 previous / current active state set comparison으로
		 *       duration notify re-entry를 완전 처리할 예정...
		 */
        return;
    }

	/**
	 * @note 일반 non-wrapped 구간에서는 이전 / 현재 시간이 duration 구간 안에 있는지만 비교.
	 * 
	 *       현재는 loop boundary를 가로지르는 state 재진입 순서를 처리하지 않으므로 위에서 wrap cleanup 후 종료함.
	 *       즉, loop로 시간이 감긴 순간, 기존에 켜져 있던 duration notify state들을 일단 정리함
	 */
	// duration notify loop
    for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Notifies.size()); ++NotifyIndex)
    {
        const FAnimNotifyEvent& Notify = Notifies[NotifyIndex];
        if (IsInstantNotify(Notify))
        {
            continue;
        }

        const bool bPrevInside = IsTimeInsideNotifyState(PreviousTime, Notify);
        const bool bCurrInside = IsTimeInsideNotifyState(CurrentTime, Notify);
        const bool bWasActive = IsNotifyStateActive(NotifyIndex);

        if (!bWasActive && bCurrInside)
        {
            AddActiveNotifyState(NotifyIndex, Notify);
            DispatchAnimNotify(Notify, EAnimNotifyPhase::Begin);
        }

        if (bCurrInside && IsNotifyStateActive(NotifyIndex))
        {
            DispatchAnimNotify(Notify, EAnimNotifyPhase::Tick);
        }

        if ((bWasActive || bPrevInside) && !bCurrInside)
        {
            RemoveActiveNotifyState(NotifyIndex, true);
        }
    }
}

void UAnimSingleNodeInstance::DispatchAnimNotify(const FAnimNotifyEvent& Notify, EAnimNotifyPhase Phase)
{
    if (USkeletalMeshComponent* Component = GetSkelMeshComponent())
    {
        FAnimNotifyDispatchEvent DispatchEvent;
        DispatchEvent.Notify = Notify;
        DispatchEvent.Phase = Phase;
        Component->HandleAnimNotify(DispatchEvent);
    }
}

void UAnimSingleNodeInstance::ClearActiveNotifyStates(bool bDispatchEnd)
{
    if (!bDispatchEnd)
    {
        ActiveNotifyStates.clear();
        return;
    }

    // End 콜백 안에서 Stop / SetPosition 등이 다시 호출될 수 있으므로, 먼저 active 목록을 비움
    const TArray<FActiveAnimNotifyState> StatesToEnd = ActiveNotifyStates;
    ActiveNotifyStates.clear();
    for (const FActiveAnimNotifyState& ActiveState : StatesToEnd)
    {
        DispatchAnimNotify(ActiveState.Notify, EAnimNotifyPhase::End);
    }
}

bool UAnimSingleNodeInstance::IsNotifyStateActive(int32 NotifyIndex) const
{
    for (const FActiveAnimNotifyState& ActiveState : ActiveNotifyStates)
    {
        if (ActiveState.NotifyIndex == NotifyIndex)
        {
            return true;
        }
    }

    return false;
}

void UAnimSingleNodeInstance::AddActiveNotifyState(int32 NotifyIndex, const FAnimNotifyEvent& Notify)
{
    if (IsNotifyStateActive(NotifyIndex))
    {
        return;
    }

    FActiveAnimNotifyState ActiveState;
    ActiveState.NotifyIndex = NotifyIndex;
    ActiveState.Notify = Notify;
    ActiveNotifyStates.push_back(ActiveState);
}

void UAnimSingleNodeInstance::RemoveActiveNotifyState(int32 NotifyIndex, bool bDispatchEnd)
{
    for (auto It = ActiveNotifyStates.begin(); It != ActiveNotifyStates.end(); ++It)
    {
        if (It->NotifyIndex != NotifyIndex)
        {
            continue;
        }

        const FAnimNotifyEvent NotifyToEnd = It->Notify;

        ActiveNotifyStates.erase(It);
        if (bDispatchEnd)
        {
            DispatchAnimNotify(NotifyToEnd, EAnimNotifyPhase::End);
        }

        return;
    }
}

bool UAnimSingleNodeInstance::EvaluateAnimation(TArray<FTransform>& OutLocalPose)
{
    SCOPE_STAT("Anim.EvaluatePose");

    USkeletalMeshComponent* Component = GetSkelMeshComponent();
    const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMesh() : nullptr;

    if (!Mesh)
    {
        return false;
    }

    if (!CurrentAsset)
    {
        return false;
    }

    // asset이 있으면 실제 sequence 평가를 시도
    if (UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(CurrentAsset))
    {
        const FAnimExtractContext ExtractContext(CurrentTime, bLooping);
        if (Sequence->GetAnimationPose(OutLocalPose, Mesh, ExtractContext))
        {
            return true;
        }

        UE_LOG_WARNING("[AnimSingleNodeInstance] Failed to evaluate animation sequence.");
        return false;
    }

    UE_LOG_WARNING("[AnimSingleNodeInstance] CurrentAsset is not a supported sequence asset.");

    return false;
}
