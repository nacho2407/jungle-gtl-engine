#pragma once
#include "Animation/AnimSequence.h"
#include "AnimInstance.h"
#include "Object/ObjectPtr.h"

class USkeletalMesh;

UCLASS()
class UAnimSingleNodeInstance : public UAnimInstance
{
public:
	GENERATED_BODY(UAnimSingleNodeInstance, UAnimInstance)
	UAnimSingleNodeInstance() = default;
	~UAnimSingleNodeInstance() override = default;

	void Serialize(FArchive& Ar) override;
	void PostEditProperty(const char* PropertyName) override;
	
	void SetAnimation(UAnimSequenceBase* InAnimation);
	void SetAnimationAssetPath(const FString& InAnimationAssetPath);
	const FString& GetAnimationAssetPath() const { return AnimationAssetPath.GetPath(); }
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
	void ApplyAnimationFromAssetPath();
	void SyncAnimationAssetPathFromAnimation(UAnimSequenceBase* Animation);

	UAnimSequenceBase* CurrentAnimation = nullptr;
	TArray<int32> TrackToBoneMap;
	USkeletalMesh* CachedMappingMesh = nullptr;
	UAnimSequenceBase* CachedMappingAnimation = nullptr;

	UPROPERTY(DisplayName = "Animation")
	TSoftObjectPtr<UAnimationAsset> AnimationAssetPath;

	UPROPERTY(DisplayName = "Play Rate", Min = "-4.0", Max = "4.0", Speed = "0.01")
	float PlayRate = 1.0f;

	bool bPlaying = false;

	UPROPERTY(DisplayName = "Loop")
	bool bLooping = false;

	UPROPERTY(DisplayName = "Auto Play")
	bool bAutoPlay = true;
};
