#include "AnimationStateMachine.h"
#include "Component/SkeletalMeshComponent.h"

void UAnimationStateMachine::Initialize(USkeletalMeshComponent* Owner)
{
	OwnerComponent = Owner;
	OwnerPawn = Cast<APawn>(OwnerComponent->GetOwner());
}

void UAnimationStateMachine::AddState(EAnimState State, UAnimSequenceBase* Sequence, float BlendTime)
{
	States[State] = {State, Sequence, BlendTime};
}

void UAnimationStateMachine::SetState(EAnimState NewState)
{
	if (CurrentState == NewState || !States.contains(NewState))
	{
		return;
	}

	PreviousState = CurrentState;
	CurrentState = NewState;

	PreviousTime = CurrentTime;
	CurrentTime = 0.0f;

	BlendDuration = States[NewState].BlendTime;
	BlendElapsed = 0.0f;

	bBlending = (BlendDuration > 0.0f && PreviousState != EAnimState::None);
}

void UAnimationStateMachine::Update(float DeltaTime)
{
	// if (CurrentState == )
}

bool UAnimationStateMachine::EvaluatePose(FPoseContext& OutPose) const
{
}

void UAnimationStateMachine::ProcessState()
{

}