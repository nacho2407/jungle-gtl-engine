#include "AnimInstance.h"
#include "Component/SkeletalMeshComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float NotifyDurationEpsilon = 0.0001f;

	bool CrossedForward(float RangeStart, float RangeEnd, float EventTime)
	{
		return RangeStart < EventTime && EventTime <= RangeEnd;
	}

	bool CrossedReverse(float RangeStart, float RangeEnd, float EventTime)
	{
		return RangeEnd <= EventTime && EventTime < RangeStart;
	}

	bool IsActiveAtTime(const FAnimNotifyStateEvent& Notify, float Time)
	{
		if (Notify.Duration <= NotifyDurationEpsilon)
		{
			return false;
		}

		return Notify.TriggerTime <= Time && Time < Notify.GetEndTime();
	}
}

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
}

void UAnimInstance::TriggerAnimNotifies(UAnimSequenceBase* Sequence, float InPreviousTime, float InCurrentTime, bool bLooped, bool bReverse, float DeltaTime)
{
	DispatchAnimNotifies(OwnerComponent, Sequence, InPreviousTime, InCurrentTime, bLooped, bReverse, DeltaTime);
}

void UAnimInstance::DispatchAnimNotifies(USkeletalMeshComponent* InOwnerComponent, UAnimSequenceBase* Sequence, float InPreviousTime, float InCurrentTime, bool bLooped, bool bReverse, float DeltaTime)
{
	if (!Sequence || !InOwnerComponent) return;

	const float Length = Sequence->GetPlayLength();
	if (Length <= 0.0f) return;

	const TArray<FAnimNotifyStateEvent>& Notifies = Sequence->GetNotifies();
	if (Notifies.empty()) return;

	auto TriggerForwardRange = [&](float Start, float End)
		{
			for (const FAnimNotifyStateEvent& Notify : Notifies)
			{
				const float NotifyStart = std::clamp(Notify.TriggerTime, 0.0f, Length);
				const float NotifyDuration = std::clamp(Notify.Duration, 0.0f, std::max(0.0f, Length - NotifyStart));
				const float NotifyEnd = NotifyStart + NotifyDuration;

				if (NotifyDuration <= NotifyDurationEpsilon)
				{
					if (CrossedForward(Start, End, NotifyStart))
					{
						InOwnerComponent->HandleAnimNotify(Notify);
					}
					continue;
				}

				const bool bBegin = CrossedForward(Start, End, NotifyStart);
				const bool bEnd = CrossedForward(Start, End, NotifyEnd);

				if (bBegin)
				{
					InOwnerComponent->HandleAnimNotifyBegin(Notify);
				}
				if (DeltaTime != 0.0f && (bBegin || IsActiveAtTime(Notify, End)))
				{
					InOwnerComponent->HandleAnimNotifyTick(Notify, std::abs(DeltaTime));
				}
				if (bEnd)
				{
					InOwnerComponent->HandleAnimNotifyEnd(Notify);
				}
			}
		};

	auto TriggerReverseRange = [&](float Start, float End)
		{
			for (const FAnimNotifyStateEvent& Notify : Notifies)
			{
				const float NotifyStart = std::clamp(Notify.TriggerTime, 0.0f, Length);
				const float NotifyDuration = std::clamp(Notify.Duration, 0.0f, std::max(0.0f, Length - NotifyStart));
				const float NotifyEnd = NotifyStart + NotifyDuration;

				if (NotifyDuration <= NotifyDurationEpsilon)
				{
					if (CrossedReverse(Start, End, NotifyStart))
					{
						InOwnerComponent->HandleAnimNotify(Notify);
					}
					continue;
				}

				const bool bEnd = CrossedReverse(Start, End, NotifyEnd);
				const bool bBegin = CrossedReverse(Start, End, NotifyStart);

				if (bEnd)
				{
					InOwnerComponent->HandleAnimNotifyEnd(Notify);
				}
				if (DeltaTime != 0.0f && (bEnd || IsActiveAtTime(Notify, End)))
				{
					InOwnerComponent->HandleAnimNotifyTick(Notify, std::abs(DeltaTime));
				}
				if (bBegin)
				{
					InOwnerComponent->HandleAnimNotifyBegin(Notify);
				}
			}
		};

	if (!bLooped)
	{
		if (!bReverse)
		{
			TriggerForwardRange(InPreviousTime, InCurrentTime);
		}
		else
		{
			TriggerReverseRange(InPreviousTime, InCurrentTime);
		}
	}
	else
	{
		if (!bReverse)
		{
			TriggerForwardRange(InPreviousTime, Length);
			TriggerForwardRange(-NotifyDurationEpsilon, InCurrentTime);
		}
		else
		{
			TriggerReverseRange(InPreviousTime, 0.0f);
			TriggerReverseRange(Length + NotifyDurationEpsilon, InCurrentTime);
		}
	}
}
