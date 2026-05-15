#pragma once
#include "AnimInstance.h"

class UAnimSingleNodeInstance : public UAnimInstance
{
public:
	DECLARE_CLASS(UAnimSingleNodeInstance, UAnimInstance)
	UAnimSingleNodeInstance() = default;
	~UAnimSingleNodeInstance() override = default;

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	void SetAnimation(UAnimSequenceBase* InAnimation);

    void Play(bool bInLooping);
    void Stop();
    void Pause();

    void SetPlayRate(float InPlayRate) { PlayRate = InPlayRate; }
    void SetLooping(bool bInLooping) { bLooping = bInLooping; }
    void SetPosition(float InPosition);

    bool IsPlaying() const { return bPlaying; }
    bool IsLooping() const { return bLooping; }
    float GetPlayRate() const { return PlayRate; }
    float GetLength() const;
    UAnimSequenceBase* GetAnimation() const { return CurrentAnimation; }

	void NativeUpdateAnimation(float DeltaTime) override;
	bool EvaluatePose(FPoseContext& OutPoseContext) override;

private:
	UAnimSequenceBase* CurrentAnimation = nullptr;
	float PlayRate = 1.0f;
	bool bPlaying = false;
    bool bLooping = false;
};