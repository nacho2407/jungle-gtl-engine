#include "ActionComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Profiling/Timer.h"
#include "Runtime/Engine.h"

IMPLEMENT_CLASS(UActionComponent, UActorComponent)

namespace
{
	FVector LerpVector(const FVector& From, const FVector& To, float Alpha)
	{
		return From + (To - From) * FMath::Clamp(Alpha, 0.0f, 1.0f);
	}

}

void UActionComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
}

void UActionComponent::EndPlay()
{
	StopAllActions();
	UActorComponent::EndPlay();
}

void UActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float RawDeltaTime = GetRawDeltaTime(DeltaTime);

	if (HitStopAction.bActive)
	{
		HitStopAction.RemainingTime -= RawDeltaTime;
		if (HitStopAction.RemainingTime <= 0.0f)
		{
			HitStopAction.bActive = false;
			HitStopAction.RemainingTime = 0.0f;
			ApplyTimeDilation();
		}
	}

	if (SlomoAction.bActive)
	{
		SlomoAction.RemainingTime -= RawDeltaTime;
		if (SlomoAction.RemainingTime <= 0.0f)
		{
			SlomoAction.bActive = false;
			SlomoAction.RemainingTime = 0.0f;
			ApplyTimeDilation();
		}
	}

	ClearTimeDilationIfIdle();

	if (HitSquashAction.bActive)
	{
		USceneComponent* TargetComponent = GetTargetSceneComponent();
		if (!TargetComponent)
		{
			HitSquashAction.bActive = false;
		}
		else
		{
			HitSquashAction.ElapsedTime += RawDeltaTime;
			const float SquashInDuration = HitSquashAction.SquashInDuration;
			const float RecoverDuration = HitSquashAction.RecoverDuration;

			if (SquashInDuration > 0.0f && HitSquashAction.ElapsedTime < SquashInDuration)
			{
				const float Alpha = HitSquashAction.ElapsedTime / SquashInDuration;
				TargetComponent->SetRelativeScale(LerpVector(HitSquashAction.StartScale, HitSquashAction.SquashedScale, Alpha));
			}
			else if (RecoverDuration > 0.0f)
			{
				const float RecoverElapsed = HitSquashAction.ElapsedTime - SquashInDuration;
				const float Alpha = RecoverElapsed / RecoverDuration;
				TargetComponent->SetRelativeScale(LerpVector(HitSquashAction.SquashedScale, HitSquashAction.StartScale, Alpha));
				if (Alpha >= 1.0f)
				{
					TargetComponent->SetRelativeScale(HitSquashAction.StartScale);
					HitSquashAction.bActive = false;
				}
			}
			else
			{
				TargetComponent->SetRelativeScale(HitSquashAction.StartScale);
				HitSquashAction.bActive = false;
			}
		}
	}

	if (KnockbackAction.bActive)
	{
		AActor* OwnerActor = GetOwner();
		if (!OwnerActor)
		{
			KnockbackAction.bActive = false;
		}
		else if (KnockbackAction.Duration <= 0.0f || KnockbackAction.RemainingTime <= 0.0f)
		{
			OwnerActor->AddActorWorldOffset(KnockbackAction.RemainingOffset);
			KnockbackAction = FKnockbackAction();
		}
		else
		{
			const float StepTime = FMath::Clamp(RawDeltaTime, 0.0f, KnockbackAction.RemainingTime);
			const float StepAlpha = StepTime / KnockbackAction.RemainingTime;
			const FVector StepOffset = KnockbackAction.RemainingOffset * StepAlpha;
			OwnerActor->AddActorWorldOffset(StepOffset);
			KnockbackAction.RemainingOffset -= StepOffset;
			KnockbackAction.RemainingTime -= StepTime;
			if (KnockbackAction.RemainingTime <= 0.0f)
			{
				OwnerActor->AddActorWorldOffset(KnockbackAction.RemainingOffset);
				KnockbackAction = FKnockbackAction();
			}
		}
	}
}

void UActionComponent::HitStop(float Duration, float TimeDilation)
{
	if (Duration <= 0.0f)
	{
		return;
	}

	CaptureBaseTimeDilation();
	HitStopAction.bActive = true;
	HitStopAction.Duration = Duration;
	HitStopAction.RemainingTime = Duration;
	HitStopAction.TimeDilation = FMath::Clamp(TimeDilation, 0.0f, 1.0f);
	ApplyTimeDilation();
}

void UActionComponent::HitSquash(const FVector& SquashedScale, float SquashInDuration, float RecoverDuration)
{
	USceneComponent* TargetComponent = GetTargetSceneComponent();
	if (!TargetComponent)
	{
		return;
	}

	HitSquashAction.bActive = true;
	HitSquashAction.SquashInDuration = SquashInDuration > 0.0f ? SquashInDuration : 0.0f;
	HitSquashAction.RecoverDuration = RecoverDuration > 0.0f ? RecoverDuration : 0.0f;
	HitSquashAction.ElapsedTime = 0.0f;
	HitSquashAction.StartScale = TargetComponent->GetRelativeScale();
	HitSquashAction.SquashedScale = SquashedScale;

	if (HitSquashAction.SquashInDuration <= 0.0f)
	{
		TargetComponent->SetRelativeScale(HitSquashAction.SquashedScale);
	}
}

void UActionComponent::Knockback(const FVector& Direction, float Distance, float Duration)
{
	if (Distance == 0.0f || Direction.IsNearlyZero())
	{
		return;
	}

	const FVector KnockbackDirection = Direction.Normalized();
	KnockbackAction.bActive = true;
	KnockbackAction.Duration = Duration > 0.0f ? Duration : 0.0f;
	KnockbackAction.RemainingTime = KnockbackAction.Duration;
	KnockbackAction.RemainingOffset = KnockbackDirection * Distance;

	if (KnockbackAction.Duration <= 0.0f)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->AddActorWorldOffset(KnockbackAction.RemainingOffset);
		}
		KnockbackAction = FKnockbackAction();
	}
}

void UActionComponent::Slomo(float Duration, float TimeDilation)
{
	if (Duration <= 0.0f)
	{
		return;
	}

	CaptureBaseTimeDilation();
	SlomoAction.bActive = true;
	SlomoAction.Duration = Duration;
	SlomoAction.RemainingTime = Duration;
	SlomoAction.TimeDilation = FMath::Clamp(TimeDilation, 0.0f, 1.0f);
	ApplyTimeDilation();
}

void UActionComponent::StopHitStop()
{
	HitStopAction = FTimedDilationAction();
	ApplyTimeDilation();
	ClearTimeDilationIfIdle();
}

void UActionComponent::StopHitSquash()
{
	if (HitSquashAction.bActive)
	{
		if (USceneComponent* TargetComponent = GetTargetSceneComponent())
		{
			TargetComponent->SetRelativeScale(HitSquashAction.StartScale);
		}
	}
	HitSquashAction = FHitSquashAction();
}

void UActionComponent::StopKnockback()
{
	KnockbackAction = FKnockbackAction();
}

void UActionComponent::StopSlomo()
{
	SlomoAction = FTimedDilationAction();
	ApplyTimeDilation();
	ClearTimeDilationIfIdle();
}

void UActionComponent::StopAllActions()
{
	StopHitSquash();
	StopKnockback();
	HitStopAction = FTimedDilationAction();
	SlomoAction = FTimedDilationAction();
	if (bHasCapturedBaseTimeDilation)
	{
		if (GEngine && GEngine->GetTimer())
		{
			GEngine->GetTimer()->SetTimeDilation(BaseTimeDilation);
		}
		bHasCapturedBaseTimeDilation = false;
	}
}

float UActionComponent::GetRawDeltaTime(float FallbackDeltaTime) const
{
	if (GEngine && GEngine->GetTimer())
	{
		return GEngine->GetTimer()->GetRawDeltaTime();
	}
	return FallbackDeltaTime;
}

USceneComponent* UActionComponent::GetTargetSceneComponent() const
{
	AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
}

void UActionComponent::CaptureBaseTimeDilation()
{
	if (bHasCapturedBaseTimeDilation)
	{
		return;
	}

	if (GEngine && GEngine->GetTimer())
	{
		BaseTimeDilation = GEngine->GetTimer()->GetTimeDilation();
	}
	else
	{
		BaseTimeDilation = 1.0f;
	}
	bHasCapturedBaseTimeDilation = true;
}

void UActionComponent::ApplyTimeDilation()
{
	if (!GEngine || !GEngine->GetTimer())
	{
		return;
	}

	if (HasActiveTimeDilationAction(HitStopAction))
	{
		GEngine->GetTimer()->SetTimeDilation(HitStopAction.TimeDilation);
		return;
	}

	if (HasActiveTimeDilationAction(SlomoAction))
	{
		GEngine->GetTimer()->SetTimeDilation(SlomoAction.TimeDilation);
		return;
	}

	if (bHasCapturedBaseTimeDilation)
	{
		GEngine->GetTimer()->SetTimeDilation(BaseTimeDilation);
	}
}

void UActionComponent::ClearTimeDilationIfIdle()
{
	if (!HasActiveTimeDilationAction(HitStopAction) && !HasActiveTimeDilationAction(SlomoAction))
	{
		bHasCapturedBaseTimeDilation = false;
	}
}

bool UActionComponent::HasActiveTimeDilationAction(const FTimedDilationAction& Action) const
{
	return Action.bActive && Action.RemainingTime > 0.0f;
}
