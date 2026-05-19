#include "AnimInstance.h"
#include "Component/SkeletalMeshComponent.h"


void UAnimInstance::Initialize(USkeletalMeshComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
}

void UAnimInstance::TriggerAnimNotifies(UAnimSequenceBase* Sequence, float InPreviousTime, float InCurrentTime, bool bLooped, bool bReverse)
{
	DispatchAnimNotifies(OwnerComponent, Sequence, InPreviousTime, InCurrentTime, bLooped, bReverse);
}

void UAnimInstance::DispatchAnimNotifies(USkeletalMeshComponent* InOwnerComponent, UAnimSequenceBase* Sequence, float InPreviousTime, float InCurrentTime, bool bLooped, bool bReverse)
{
	if (!Sequence || !InOwnerComponent) return;

	const float Length = Sequence->GetPlayLength();
	if (Length <= 0.0f) return;

	const TArray<FAnimNotifyEvent>& Notifies = Sequence->GetNotifies();
	if (Notifies.empty()) return;

	auto TriggerRange = [&](float Start, float End)
		{
			for (const FAnimNotifyEvent& Notify : Notifies)
			{
				const float T = Notify.TriggerTime;
				const bool bHit = !bReverse ? (Start < T && T <= End) : (End <= T && T < Start);

				if (bHit)
				{
					InOwnerComponent->HandleAnimNotify(Notify);
				}
			}
		};

	if (!bLooped)
	{
		TriggerRange(InPreviousTime, InCurrentTime);
	}
	else
	{
		if (!bReverse)
		{
			TriggerRange(InPreviousTime, Length);
			TriggerRange(0.0f, InCurrentTime);
		}
		else
		{
			TriggerRange(InPreviousTime, 0.0f);
			TriggerRange(Length, InCurrentTime);
		}
	}
}
