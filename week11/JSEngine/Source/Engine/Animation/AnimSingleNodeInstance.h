#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimationTypes.h"

class UAnimationAsset;

class UAnimSingleNodeInstance : public UAnimInstance
{
public:
    DECLARE_CLASS(UAnimSingleNodeInstance, UAnimInstance)

    UAnimSingleNodeInstance() = default;
    ~UAnimSingleNodeInstance() override = default;

    void SetAnimationAsset(UAnimationAsset* NewAsset);
    UAnimationAsset* GetAnimationAsset() const;

    void Play();
    void Pause();
    void Stop();

    void SetPosition(float InTimeSeconds, bool bFireNotifies = false);
    float GetPosition() const;
    float GetPreviousTime() const;
    float GetPlayLength() const;

    void SetPlayRate(float InPlayRate);
    float GetPlayRate() const;
    void SetReversePlay(bool bInReversePlay);
    bool IsReversePlay() const;

    void SetLooping(bool bInLooping);
    bool IsLooping() const;
    bool IsPlaying() const;
    bool IsPaused() const;
    void SetRootMotionMode(ERootMotionMode InMode);
    ERootMotionMode GetRootMotionMode() const;
    const FRootMotionDelta& GetLastExtractedRootMotion() const;
    void SetRootMotionBoneIndex(int32 InBoneIndex);
    int32 GetRootMotionBoneIndex() const;
    void SetRootMotionBoneName(const FName& InBoneName);
    FName GetRootMotionBoneName() const;

    void NativeUpdateAnimation(float DeltaSeconds) override;
    bool EvaluateAnimation(TArray<FTransform>& OutLocalPose) override;

private:
    void AdvanceTime(float DeltaSeconds);
    void ProcessRootMotion(TArray<FTransform>& InOutLocalPose);
    void ClearRootMotionState();
    void TriggerAnimNotifies();
    FAnimNotifyTriggerContext MakeNotifyTriggerContext(const UAnimSequenceBase* Sequence) const;
    void DispatchAnimNotifyEvent(const FAnimNotifyDispatchEvent& NotifyEvent);
    void ClearActiveNotifyStates(bool bDispatchEnd);

    UAnimationAsset* CurrentAsset = nullptr;
    float PreviousTime = 0.0f;
    float CurrentTime = 0.0f;
    float PlayRate = 1.0f;
    bool bLooping = false;
    bool bPlaying = false;
    bool bPaused = false;
    bool bReversePlay = false;
    bool bReachedEndThisFrame = false;
    bool bLoopedThisFrame = false;
    TArray<FActiveAnimNotifyState> ActiveNotifyStates;
    ERootMotionMode RootMotionMode = ERootMotionMode::Ignore;
    FRootMotionDelta LastExtractedRootMotion;
    int32 RootMotionBoneIndex = -1;
    FName RootMotionBoneName;

	// 다음 프레임 delta 계산을 위한 previous 값
    FTransform PreviousRootTransform = FTransform::Identity;
    bool bHasPreviousRootTransform = false;
};
