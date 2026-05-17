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
    virtual void Update(float DeltaTime) = 0;
    virtual bool EvaluatePose(FPoseContext& OutPose) const = 0;
    virtual void ResetTime() = 0;
};

// 단일 시퀀스 재생용 포즈
class FAnimSequencePoseSource : public IAnimPoseSource
{
private:
    USkeletalMeshComponent* OwnerComponent = nullptr;
    UAnimSequenceBase* Sequence = nullptr;
    float CurrentTime = 0.0f;

public:
    FAnimSequencePoseSource(USkeletalMeshComponent* InOwnerComponent, UAnimSequenceBase* InSequence)
        : OwnerComponent(InOwnerComponent), Sequence(InSequence), CurrentTime(0.0f) {}
    
    virtual void Update(float DeltaTime) override;
    virtual bool EvaluatePose(FPoseContext& OutPose) const override;
    virtual void ResetTime() override;
};

using FAnimTransitionCondition = std::function<bool()>;

// Transitions에 해당하면(bool형) ToState로 전이.　
struct FAnimTransition
{
    FName ToState;
    float BlendTime = 0.2f;
    FAnimTransitionCondition Condition;
};

// 특정 상태에 따른 name, PoseSource, 전이 목록 보유.
struct FAnimStateNode
{
    FName Name;
    std::shared_ptr<IAnimPoseSource> PoseSource;
    TArray<FAnimTransition> Transitions;
};

UCLASS()
class UAnimationStateMachine : public UObject
{
public:
    GENERATED_BODY(UAnimationStateMachine, UObject)

    void Initialize(USkeletalMeshComponent* Owner);

    void AddState(FName StateName, UAnimSequenceBase* Sequence);
    void AddTransition(FName FromState, FName ToState, float BlendTime, FAnimTransitionCondition Condition);
    void SetEntryState(FName StateName);

    void SetState(FName NewState, float BlendTime = 0.2f);

    void Update(float DeltaTime);
    bool EvaluatePose(FPoseContext& OutPose) const;


    // Lua Binding용
    void AddStateByName(const FString& StateName, UAnimSequenceBase* Sequence);
    void AddStateFromPath(const FString& StateName, const FString& AnimPath);

    void SetEntryStateByName(const FString& StateName);
    void SetStateByName(const FString& StateName, float BlendTime = 0.2f);

    FString GetCurrentStateName() const;
    FString GetNextStateName() const;
    bool IsBlending() const { return bBlending; }

private:
    TMap<FName, FAnimStateNode, FName::Hash> States;

    FName CurrentState;
    FName NextState;

    bool bBlending = false;
    float BlendElapsed = 0.0f;
    float BlendDuration = 0.0f;

    USkeletalMeshComponent* OwnerComponent = nullptr;
    APawn* OwnerPawn = nullptr;
};
