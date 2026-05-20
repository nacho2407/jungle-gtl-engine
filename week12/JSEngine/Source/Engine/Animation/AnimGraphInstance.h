#pragma once
#include "AnimationStateMachine.h"
#include "Core/CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimationStateMachine.h"

#include <unordered_set>

class UAnimSequence;
class USkeletalMesh;

struct FAnimGraphStateMachineCache
{
	UAnimationStateMachine* RuntimeMachine = nullptr;
	FString Signature;
};

struct FAnimGraphSequenceCache
{
	UAnimSequence* Sequence = nullptr;
	FString AnimationPath;
	TArray<int32> TrackToBoneMap;
	USkeletalMesh* CachedMesh = nullptr;
	float PreviousTime = 0.0f;
	float CurrentTime = 0.0f;
};

UCLASS()
class UAnimGraphInstance : public UAnimInstance
{
public:
	GENERATED_BODY(UAnimGraphInstance, UAnimInstance)

	void Serialize(FArchive& Ar) override;
	void SetGraphAsset(UAnimGraphAsset* InAsset);
	void CopyRuntimeParametersFrom(const UAnimGraphInstance* SourceInstance);

	virtual void NativeUpdateAnimation(float DeltaTime) override;
	virtual bool EvaluatePose(FPoseContext& OutPoseContext) override;

	void SetFloatParameter(const FString& Name, float Value);
	void SetBoolParameter(const FString& Name, bool Value);
	void SetIntParameter(const FString& Name, int32 Value);
	float GetFloatParameter(const FString& Name) const;
	bool GetBoolParameter(const FString& Name) const;
	int32 GetIntParameter(const FString& Name) const;


private:
	void UpdateNode(int32 NodeId, float DeltaTime);
	void UpdateSequencePlayer(const FAnimGraphNodeDesc& Node, float DeltaTime);
	bool EvaluateNode(int32 NodeId, FPoseContext& OutPoseContext);
	bool EvaluateSequencePlayer(const FAnimGraphNodeDesc& Node, FPoseContext& OutPoseContext);
	bool LogNodeWarningOnce(int32 NodeId, const FString& Message);

	FAnimGraphSequenceCache& GetOrCreateSequenceCache(int32 NodeId, const FString& AnimationPath);
	void BuildBoneMapping(FAnimGraphSequenceCache& Cache);

	bool EvaluateStateMachine(const FAnimGraphNodeDesc& Node, FPoseContext& OutPoseContext);

	FAnimGraphStateMachineCache& GetOrCreateStateMachineCache(const FAnimGraphNodeDesc& Node);
	UAnimationStateMachine* BuildStateMachineRuntime(const FAnimStateMachineDesc& Desc);
	FAnimTransitionCondition BuildConditionFunction(const FAnimTransitionConditionDesc& Desc);
	FString BuildStateMachineSignature(const FAnimStateMachineDesc& Desc) const;

private:
	UAnimGraphAsset* GraphAsset = nullptr;

	TMap<int32, FAnimGraphSequenceCache> SequenceCacheMap;
	TMap<int32, FAnimGraphStateMachineCache> StateMachineCacheMap;

	TMap<FString, float> FloatParameters;
	TMap<FString, bool> BoolParameters;
	TMap<FString, int32> IntParameters;

	bool bLoggedMissingGraph = false;
	std::unordered_set<int32> LoggedNodeWarnings;
};
