#pragma once
#include "AnimInstance.h"

class USkeletalMesh;

UCLASS()
class UAnimSingleNodeInstance : public UAnimInstance
{
public:
	GENERATED_BODY(UAnimSingleNodeInstance, UAnimInstance)
	UAnimSingleNodeInstance() = default;
	~UAnimSingleNodeInstance() override = default;
	
	void SetAnimation(UAnimSequenceBase* InAnimation);
	void Initialize(USkeletalMeshComponent* InOwnerComponent) override;
	void BuildBoneMapping();

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
	bool NeedsBoneMappingRebuild() const;

	UAnimSequenceBase* CurrentAnimation = nullptr;
	TArray<int32> TrackToBoneMap;
	USkeletalMesh* CachedMappingMesh = nullptr;
	UAnimSequenceBase* CachedMappingAnimation = nullptr;
	float PlayRate = 1.0f;
	bool bPlaying = false;
	bool bLooping = false;
};