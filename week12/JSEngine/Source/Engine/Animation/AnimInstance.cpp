#include "AnimInstance.h"
#include "Component/SkeletalMeshComponent.h"

DEFINE_CLASS(UAnimInstance, UObject)

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
}

void UAnimInstance::TriggerAnimNotifies(UAnimSequenceBase* Sequence, float InPreviousTime, float InCurrentTime, bool bLooped, bool bReverse)
{
	if (!Sequence || !OwnerComponent) return;

	const float Length = Sequence->GetPlayLength();
	const TArray<FAnimNotifyEvent>& Notifies = Sequence->GetNotifies();

	auto TriggerRange = [&](float Start, float End)
		{
			for (const FAnimNotifyEvent& Notify : Notifies)
			{
				const float T = Notify.TriggerTime;
				const bool bHit = !bReverse ? (Start < T && T <= End) : (End <= T && T < Start);

				if (bHit)
				{
					OwnerComponent->HandleAnimNotify(Notify);
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