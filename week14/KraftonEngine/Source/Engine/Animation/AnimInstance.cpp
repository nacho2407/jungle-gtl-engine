#include "AnimInstance.h"
#include "AnimSequenceBase.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"

DEFINE_CLASS(UAnimInstance, UObject)

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	NativeUpdateAnimation(DeltaSeconds);
}

void UAnimInstance::EvaluatePose(FPoseContext& Output)
{
	EvaluateAnimation(Output);
}

USkeletalMesh* UAnimInstance::GetSkeletalMesh() const
{
	return OwningComponent ? OwningComponent->GetSkeletalMesh() : nullptr;
}

void UAnimInstance::TriggerAnimNotifies(float PreviousTime, float CurrentTime, const UAnimSequenceBase* Sequence)
{
	if (!Sequence) return;

	const TArray<FAnimNotifyEvent>& Notifies = Sequence->GetNotifies();
	const float Length = Sequence->GetPlayLength();
	const bool  bWrapped = (CurrentTime < PreviousTime); // 루프로 시간 wrap

	auto InRange = [&](float Trigger) -> bool
	{
		if (!bWrapped)
		{
			return Trigger >= PreviousTime && Trigger < CurrentTime;
		}
		// wrap: [Prev, Length) ∪ [0, Current)
		return (Trigger >= PreviousTime && Trigger < Length) ||
		       (Trigger >= 0.0f         && Trigger < CurrentTime);
	};

	for (const FAnimNotifyEvent& Notify : Notifies)
	{
		if (InRange(Notify.TriggerTime))
		{
			HandleAnimNotify(Notify);
		}
	}
}
