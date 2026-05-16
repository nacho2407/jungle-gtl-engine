#pragma once

#include "AnimInstance.h"

class UAnimationStateMachine;

class UStateMachineAnimInstance : public UAnimInstance
{
public:
	DECLARE_CLASS(UStateMachineAnimInstance, UAnimInstance)

	void SetStateMachine(UAnimationStateMachine* InStateMachine);

	virtual void NativeUpdateAnimation(float DeltaTime) override;
	virtual bool EvaluatePose(FPoseContext& OutPoseContext) override;

private:
	UAnimationStateMachine* StateMachine = nullptr;
};