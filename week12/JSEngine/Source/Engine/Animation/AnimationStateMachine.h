#pragma once

#include "AnimSequence.h"
#include "Core/CoreMinimal.h"
#include "AnimTypes.h"
#include "GameFramework/Pawn.h"

class USkeletalMeshComponent;

// Pose 소스 인터페이스
class IAnimPoseSource
{
public:
	virtual ~IAnimPoseSource() = default;
	virtual void SetOwnerComponent(USkeletalMeshComponent* InOwnerComponent) = 0;
	virtual void Update(float DeltaTime) = 0;
	virtual bool EvaluatePose(FPoseContext& OutPose) const = 0;
	virtual void ResetTime() = 0;
	virtual bool IsFinished() const = 0;
	virtual bool IsLooping() const = 0;
};

// 단일 시퀀스 재생용 포즈
class FAnimSequencePoseSource : public IAnimPoseSource
{
private:
	USkeletalMeshComponent* OwnerComponent = nullptr;
	UAnimSequenceBase* Sequence = nullptr;
	float CurrentTime = 0.0f;
	float PreviousTime = 0.0f;
	float PlayRate = 1.0f;
	bool bLoop = true;
	bool bFinished = false;

public:
	FAnimSequencePoseSource(USkeletalMeshComponent* InOwnerComponent, UAnimSequenceBase* InSequence, float InPlayRate = 1.0f, bool bInLoop = true)
		: OwnerComponent(InOwnerComponent)
		, Sequence(InSequence)
		, CurrentTime(0.0f)
		, PreviousTime(0.0f)
		, PlayRate(InPlayRate)
		, bLoop(bInLoop)
		, bFinished(false)
	{
	}

	virtual void SetOwnerComponent(USkeletalMeshComponent* InOwnerComponent) override { OwnerComponent = InOwnerComponent; }
	virtual void Update(float DeltaTime) override;
	virtual bool EvaluatePose(FPoseContext& OutPose) const override;
	virtual void ResetTime() override;
	virtual bool IsFinished() const override { return !bLoop && bFinished; }
	virtual bool IsLooping() const override { return bLoop; }
};

using FAnimTransitionCondition = std::function<bool()>;

// Transitions에 해당하면(bool형) ToState로 전이.　
struct FAnimTransition
{
	FName ToState;
	float BlendTime = 0.2f;
	int32 Priority = 0;
	FAnimTransitionCondition Condition;
};

// 특정 상태에 따른 name, PoseSource, 전이 목록 보유.
struct FAnimStateNode
{
	FName Name;
	std::shared_ptr<IAnimPoseSource> PoseSource;
	TArray<FAnimTransition> Transitions;
	FString AnimationPath;
	float PlayRate = 1.0f;
	bool bLoop = true;
	bool bAutoAdvanceOnEnd = true;
};

UCLASS()
class UAnimationStateMachine : public UObject
{
public:
	GENERATED_BODY(UAnimationStateMachine, UObject)

	void Serialize(FArchive& Ar) override;
	void Initialize(USkeletalMeshComponent* Owner);
	void CopyRuntimeStateFrom(const UAnimationStateMachine* SourceMachine);

	void AddState(FName StateName, UAnimSequenceBase* Sequence, float PlayRate = 1.0f, bool bLoop = true, bool bAutoAdvanceOnEnd = true);
	void AddTransition(FName FromState, FName ToState, float BlendTime, FAnimTransitionCondition Condition, int32 Priority = 0);
	void ClearTransitions();
	void SetEntryState(FName StateName);

	void SetState(FName NewState, float BlendTime = 0.2f);

	void Update(float DeltaTime);
	bool EvaluatePose(FPoseContext& OutPose) const;


	// Lua Binding용
	void AddStateByName(const FString& StateName, UAnimSequenceBase* Sequence);
	void AddStateFromPath(const FString& StateName, const FString& AnimPath);
	void AddStateByNameWithPlayback(const FString& StateName, UAnimSequenceBase* Sequence, float PlayRate, bool bLoop, bool bAutoAdvanceOnEnd);
	void AddStateFromPathWithPlayback(const FString& StateName, const FString& AnimPath, float PlayRate, bool bLoop, bool bAutoAdvanceOnEnd);

	void SetEntryStateByName(const FString& StateName);
	void SetStateByName(const FString& StateName, float BlendTime = 0.2f);

	FString GetCurrentStateName() const;
	FString GetNextStateName() const;
	bool IsBlending() const { return bBlending; }

	TArray<FString> GetStateNames() const;
	float GetBlendAlpha() const;
	float GetBlendDuration() const { return BlendDuration; }
	float GetBlendElapsed() const { return BlendElapsed; }

private:
	bool TryStartTransitionFromCurrentState();

private:
	TMap<FName, FAnimStateNode, FName::Hash> States;

	FName CurrentState;
	FName NextState;

	bool bBlending = false;
	float BlendElapsed = 0.0f;
	float BlendDuration = 0.0f;

	TArray<FName> StateOrder;

	USkeletalMeshComponent* OwnerComponent = nullptr;
	APawn* OwnerPawn = nullptr;
};
