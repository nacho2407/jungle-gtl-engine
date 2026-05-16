#include "StateMachineAnimInstance.h"
#include "AnimationStateMachine.h"

DEFINE_CLASS(UStateMachineAnimInstance, UAnimInstance)

void UStateMachineAnimInstance::SetStateMachine(UAnimationStateMachine* InStateMachine)
{
    StateMachine = InStateMachine;
}

void UStateMachineAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
    if (StateMachine)
    {
        StateMachine->Update(DeltaTime);
    }
}

bool UStateMachineAnimInstance::EvaluatePose(FPoseContext& OutPoseContext)
{
	return StateMachine ? StateMachine->EvaluatePose(OutPoseContext) : false;
}