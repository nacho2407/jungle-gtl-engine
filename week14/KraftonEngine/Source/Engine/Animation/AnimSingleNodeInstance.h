#pragma once

#include "AnimInstance.h"

class UAnimSequenceBase;

// 시퀀스 1개를 재생하는 가장 단순한 인스턴스.
// PlayRate 음수 → Reverse Play (Notion 데모용 옵션).

#include "Source/Engine/Animation/AnimSingleNodeInstance.generated.h"

UCLASS()
class UAnimSingleNodeInstance : public UAnimInstance
{
public:
	GENERATED_BODY()
	UAnimSingleNodeInstance() = default;
	~UAnimSingleNodeInstance() override = default;

	void SetAnimationAsset(UAnimSequenceBase* InAsset);
	UAnimSequenceBase* GetAnimationAsset() const { return CurrentAsset; }

	void  SetPlayRate(float InRate) { PlayRate = InRate; }
	float GetPlayRate() const       { return PlayRate; }

	void  SetLooping(bool bInLoop) { bLooping = bInLoop; }
	bool  IsLooping() const        { return bLooping; }

	void  SetPlaying(bool bInPlay) { bPlaying = bInPlay; }
	bool  IsPlaying() const        { return bPlaying; }

	float GetCurrentTime() const   { return CurrentTime; }
	void  SetCurrentTime(float T)  { CurrentTime = T; }

	// UAnimInstance:
	void NativeUpdateAnimation(float DeltaSeconds) override;
	void EvaluateAnimation(FPoseContext& Output) override;

private:
	// CurrentTime 을 dt * PlayRate 만큼 진행시키고 길이로 wrap/clamp.
	void AdvanceTime(float DeltaSeconds);

	UAnimSequenceBase* CurrentAsset = nullptr;
	float CurrentTime = 0.0f;
	float PlayRate    = 1.0f;
	bool  bPlaying    = true;
	bool  bLooping    = true;
};
