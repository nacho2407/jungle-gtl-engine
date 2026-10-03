#include "Animation/AnimSingleNodeInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationRuntime.h"
#include "Asset/SkeletalMesh.h"
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

FName ResolveNotifySourceAnimationName(const UAnimationAsset* Asset)
{
    if (!Asset)
    {
        return FName();
    }

    // 실제 사용자가 보는 clip 이름에 가까운 이름을 사용하기 위해 FBX AnimStack 이름이 있으면 우선 사용
    if (const UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
    {
        if (!Sequence->AnimStackName.empty())
        {
            return FName(Sequence->AnimStackName);
        }
    }

    return Asset->GetFName();
}

} // namespace

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimationAsset* NewAsset)
{
    if (CurrentAsset == NewAsset)
    {
        if (!NewAsset)
        {
            ClearActiveNotifyStates(true);
            ClearRootMotionState();
            bPlaying = false;
            bPaused = false;
            bReachedEndThisFrame = false;
            bLoopedThisFrame = false;
            if (USkeletalMeshComponent* Component = GetSkelMeshComponent())
            {
                Component->ClearLastAnimNotifyEvent();
            }
        }
        return;
    }

    ClearActiveNotifyStates(true);
    ClearRootMotionState();
    if (USkeletalMeshComponent* Component = GetSkelMeshComponent())
    {
        Component->ClearLastAnimNotifyEvent();
    }

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
    ClearRootMotionState();

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
    ClearRootMotionState();

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

float UAnimSingleNodeInstance::GetPreviousTime() const
{
    return PreviousTime;
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

    FAnimationRuntime::TriggerAnimNotifies(
        MakeNotifyTriggerContext(Sequence),
        ActiveNotifyStates,
        [this](const FAnimNotifyDispatchEvent& NotifyEvent)
        {
            DispatchAnimNotifyEvent(NotifyEvent);
        });
}

void UAnimSingleNodeInstance::ClearActiveNotifyStates(bool bDispatchEnd)
{
    FAnimationRuntime::ClearActiveAnimNotifyStates(
        ActiveNotifyStates,
        [this](const FAnimNotifyDispatchEvent& NotifyEvent)
        {
            DispatchAnimNotifyEvent(NotifyEvent);
        },
        bDispatchEnd,
        MakeNotifyTriggerContext(Cast<UAnimSequenceBase>(CurrentAsset)));
}

FAnimNotifyTriggerContext UAnimSingleNodeInstance::MakeNotifyTriggerContext(const UAnimSequenceBase* Sequence) const
{
    FAnimNotifyTriggerContext Context;
    Context.Sequence = Sequence;
    Context.PreviousTime = PreviousTime;
    Context.CurrentTime = CurrentTime;
    Context.bLooping = bLooping;
    Context.bReverse = PlayRate < 0.0f;
    Context.bLooped = bLoopedThisFrame;
    Context.TriggerWeight = 1.0f;
    Context.TriggerWeightThreshold = 0.0f;
    Context.SourceAnimationName = ResolveNotifySourceAnimationName(CurrentAsset);
    return Context;
}

void UAnimSingleNodeInstance::DispatchAnimNotifyEvent(const FAnimNotifyDispatchEvent& NotifyEvent)
{
    if (USkeletalMeshComponent* Component = GetSkelMeshComponent())
    {
        Component->HandleAnimNotify(NotifyEvent);
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
            ProcessRootMotion(OutLocalPose, bLoopedThisFrame);
            return true;
        }

        UE_LOG_WARNING("[AnimSingleNodeInstance] Failed to evaluate animation sequence.");
        return false;
    }

    UE_LOG_WARNING("[AnimSingleNodeInstance] CurrentAsset is not a supported sequence asset.");

    return false;
}
