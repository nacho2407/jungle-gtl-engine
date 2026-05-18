#pragma once

#include "Animation/AnimInstance.h"

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
    float GetPlayLength() const;

    void SetPlayRate(float InPlayRate);
    float GetPlayRate() const;
    void SetReversePlay(bool bInReversePlay);
    bool IsReversePlay() const;

    void SetLooping(bool bInLooping);
    bool IsLooping() const;
    bool IsPlaying() const;
    bool IsPaused() const;

    void NativeUpdateAnimation(float DeltaSeconds) override;
    bool EvaluateAnimation(TArray<FTransform>& OutLocalPose) override;

private:
    void AdvanceTime(float DeltaSeconds);

private:
    UAnimationAsset* CurrentAsset = nullptr;
    float PreviousTime = 0.0f; // animation notify 시점 계산을 위해 필요
    float CurrentTime = 0.0f;
    float PlayRate = 1.0f;
    bool bLooping = false;
    bool bPlaying = false;
    bool bPaused = false;
    bool bReversePlay = false;
    bool bReachedEndThisFrame = false;
    bool bLoopedThisFrame = false;
};
