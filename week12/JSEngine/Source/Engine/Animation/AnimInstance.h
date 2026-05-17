#pragma once

#include "AnimSequence.h"

class USkeletalMeshComponent;

UCLASS()
class UAnimInstance : public UObject
{
public:
	GENERATED_BODY(UAnimInstance, UObject)
	UAnimInstance() = default;
	~UAnimInstance() override = default;
	
	virtual void Initialize(USkeletalMeshComponent* InOwnerComponent);
	virtual void NativeUpdateAnimation(float DeltaTime) {};
	virtual bool EvaluatePose(FPoseContext& OutPoseContext) { return false; }

	float GetCurrentTime() const { return CurrentTime; }
	float GetPreviousTime() const { return PreviousTime; }
	USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

	void TriggerAnimNotifies(UAnimSequenceBase* Sequence, float InPreviousTime, float InCurrentTime, bool bLooped, bool bReverse);

protected:
	USkeletalMeshComponent* OwnerComponent = nullptr;
	float CurrentTime = 0.0f;
	float PreviousTime = 0.0f;
};
