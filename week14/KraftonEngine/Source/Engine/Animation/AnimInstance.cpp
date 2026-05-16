#include "AnimInstance.h"
#include "AnimNotify.h"
#include "AnimSequenceBase.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"

DEFINE_CLASS(UAnimInstance, UObject)

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	// 자식이 시간 진행 + AddAnimNotifies 로 큐에 적재 → 베이스가 일괄 dispatch.
	NativeUpdateAnimation(DeltaSeconds);
	DispatchQueuedAnimEvents();
}

void UAnimInstance::EvaluatePose(FPoseContext& Output)
{
	EvaluateAnimation(Output);
}

USkeletalMesh* UAnimInstance::GetSkeletalMesh() const
{
	return OwningComponent ? OwningComponent->GetSkeletalMesh() : nullptr;
}

void UAnimInstance::AddAnimNotifies(float PreviousTime, float CurrentTime, const UAnimSequenceBase* Sequence)
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
			NotifyQueue.push_back({ Notify, Sequence });
		}
	}
}

void UAnimInstance::DispatchQueuedAnimEvents()
{
	for (const FQueuedAnimNotify& Q : NotifyQueue)
	{
		// 1) UE 패턴 — 로직 객체가 박혀 있으면 자기 Notify() 실행. 시퀀스가 자기 로직 소유.
		if (Q.Event.Notify)
		{
			// UAnimNotify::Notify 시그니처가 비-const 라 const_cast.
			Q.Event.Notify->Notify(OwningComponent, const_cast<UAnimSequenceBase*>(Q.Sequence));
		}

		// 2) AnimInstance 자식이 NotifyName 매칭으로 추가 처리할 수 있도록 fallback 후크.
		HandleAnimNotify(Q.Event);
	}
	NotifyQueue.clear();
}
