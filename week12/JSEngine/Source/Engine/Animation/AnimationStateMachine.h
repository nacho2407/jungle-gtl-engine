#pragma once

#include "AnimSequence.h"
#include "Core/CoreMinimal.h"
#include "AnimTypes.h"
#include "GameFramework/Pawn.h"

class USkeletalMeshComponent;

enum class EAnimState
{
    None,
    Idle,
    Walk,
    Run,
    Fly
};

struct FAnimStateNode
{
    EAnimState State;
    UAnimSequenceBase* Sequence = nullptr;
    float BlendTime = 0.2f;
};

class UAnimationStateMachine : public UObject
{
public:
    void Initialize(USkeletalMeshComponent* Owner);

    void AddState(EAnimState State, UAnimSequenceBase* Sequence, float BlendTime = 0.2f);
    void SetState(EAnimState NewState);

    void Update(float DeltaTime);
    bool EvaluatePose(FPoseContext& OutPose) const;

private:
    void ProcessState();

private:
    TMap<EAnimState, FAnimStateNode> States;

    EAnimState CurrentState = EAnimState::None;
    EAnimState PreviousState = EAnimState::None;

    float CurrentTime = 0.0f;
    float PreviousTime = 0.0f;

    bool bBlending = false;
    float BlendElapsed = 0.0f;
    float BlendDuration = 0.0f;

    USkeletalMeshComponent* OwnerComponent = nullptr;
    APawn* OwnerPawn = nullptr;
};