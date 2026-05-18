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
    void TriggerAnimNotifies();
    void DispatchAnimNotify(const FAnimNotifyEvent& Notify, EAnimNotifyPhase Phase);
    void ClearActiveNotifyStates(bool bDispatchEnd);
    bool IsNotifyStateActive(int32 NotifyIndex) const;
    void AddActiveNotifyState(int32 NotifyIndex, const FAnimNotifyEvent& Notify);
    void RemoveActiveNotifyState(int32 NotifyIndex, bool bDispatchEnd);

private:
    struct FActiveAnimNotifyState
    {
        /**
		 * @brief 현재 active인 duration notify가 원본 notify 배열의 몇 번째 notify인지 식별하기 위해 필요
		 * 
		 * @note duration notify는 Begin 한 번 보내고 끝이 아니라 이후 프레임에서 계속 추적할 수 있어야 함.
		 *       '이미 active 중인데 Begin을 또 보내면 안 되는가' 등을 판단하려면 이 notify가 이미 active 목록에
		 *       들어가 있는지를 빠르게 확인할 식별자가 필요함
		 */
        int32 NotifyIndex = -1;
        FAnimNotifyEvent Notify;
    };

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
};
