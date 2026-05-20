#pragma once

#include "AnimInstance.h"

class UAnimationStateMachine;

UCLASS()
class UStateMachineAnimInstance : public UAnimInstance
{
public:
	GENERATED_BODY(UStateMachineAnimInstance, UAnimInstance)

	void Serialize(FArchive& Ar) override;
	void Initialize(USkeletalMeshComponent* InOwnerComponent) override;
	void SetStateMachine(UAnimationStateMachine* InStateMachine);
	UAnimationStateMachine* GetStateMachine() const { return StateMachine; }

	virtual void NativeUpdateAnimation(float DeltaTime) override;
	virtual bool EvaluatePose(FPoseContext& OutPoseContext) override;

private:
	UAnimationStateMachine* StateMachine = nullptr;
};
