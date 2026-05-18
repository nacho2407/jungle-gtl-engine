#pragma once
#include "Core/CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimGraphAsset.h"

class UAnimSequence;
class USkeletalMesh;

struct FAnimGraphSequenceCache
{
	UAnimSequence* Sequence = nullptr;
	TArray<int32> TrackToBoneMap;
	USkeletalMesh* CachedMesh = nullptr;
};

UCLASS()
class UAnimGraphInstance : public UAnimInstance
{
public:
	GENERATED_BODY(UAnimGraphInstance, UAnimInstance)

	void SetGraphAsset(UAnimGraphAsset* InAsset);

	virtual void NativeUpdateAnimation(float DeltaTime) override;
	virtual bool EvaluatePose(FPoseContext& OutPoseContext) override;

	void SetFloatParameter(const FString& Name, float Value);
	void SetBoolParameter(const FString& Name, bool Value);
	float GetFloatParameter(const FString& Name) const;
	bool GetBoolParameter(const FString& Name) const;


private:
	bool EvaluateNode(int32 NodeId, FPoseContext& OutPoseContext);
	bool EvaluateSequencePlayer(const FAnimGraphNodeDesc& Node, FPoseContext& OutPoseContext);

	FAnimGraphSequenceCache& GetOrCreateSequenceCache(int32 NodeId, const FString& AnimationPath);
	void BuildBoneMapping(FAnimGraphSequenceCache& Cache);

	// MVP 이후 확장될 StateMachine 평가용
	// bool EvaluateStateMachine(const FAnimGraphNodeDesc& Node, FPoseContext& OutPoseContext);

private:
	UAnimGraphAsset* GraphAsset = nullptr;

	TMap<int32, FAnimGraphSequenceCache> SequenceCacheMap;

	TMap<FString, float> FloatParameters;
	TMap<FString, bool> BoolParameters;
};
