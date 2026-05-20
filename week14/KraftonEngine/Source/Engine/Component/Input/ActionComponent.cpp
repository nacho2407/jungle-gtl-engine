#include "ActionComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"
#include "Profiling/Time/Timer.h"
#include "Runtime/Engine.h"

#include <algorithm>

TArray<UActionComponent*> UActionComponent::TimeDilationComponents;
bool UActionComponent::bHasCapturedGlobalBaseTimeDilation = false;
float UActionComponent::GlobalBaseTimeDilation = 1.0f;

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
			UpdateTimeDilationRegistration();
		}
	}

	if (SlomoAction.bActive)
	{
		SlomoAction.RemainingTime -= RawDeltaTime;
		if (SlomoAction.RemainingTime <= 0.0f)
		{
			SlomoAction.bActive = false;
			SlomoAction.RemainingTime = 0.0f;
			UpdateTimeDilationRegistration();
		}
	}

	if (LocalHitStopAction.bActive)
	{
		LocalHitStopAction.RemainingTime -= RawDeltaTime;
		if (LocalHitStopAction.RemainingTime <= 0.0f)
		{
			StopLocalHitStop();
		}
	}

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

	HitStopAction.bActive = true;
	HitStopAction.Duration = Duration;
	HitStopAction.RemainingTime = Duration;
	HitStopAction.TimeDilation = FMath::Clamp(TimeDilation, 0.0f, 1.0f);
	UpdateTimeDilationRegistration();
}

void UActionComponent::LocalHitStop(float Duration)
{
	if (Duration <= 0.0f)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	if (LocalHitStopAction.bActive)
	{
		LocalHitStopAction.RemainingTime = (std::max)(LocalHitStopAction.RemainingTime, Duration);
		LocalHitStopAction.Duration = (std::max)(LocalHitStopAction.Duration, Duration);
		return;
	}

	LocalHitStopAction.bActive = true;
	LocalHitStopAction.Duration = Duration;
	LocalHitStopAction.RemainingTime = Duration;
	LocalHitStopAction.bActorTickWasEnabled = OwnerActor->PrimaryActorTick.bTickEnabled;
	LocalHitStopAction.ComponentTickStates.clear();

	OwnerActor->PrimaryActorTick.SetTickEnabled(false);

	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		if (!Component || Component == this)
		{
			continue;
		}

		LocalHitStopAction.ComponentTickStates.push_back({ Component, Component->PrimaryComponentTick.bTickEnabled });
		Component->SetComponentTickEnabled(false);
	}
}

void UActionComponent::HitSquash(const FVector& SquashedScale, float SquashInDuration, float RecoverDuration)
{
	USceneComponent* TargetComponent = GetTargetSceneComponent();
	if (!TargetComponent)
	{
		return;
	}

	const FVector OriginalScale = HitSquashAction.bActive
		? HitSquashAction.StartScale
		: TargetComponent->GetRelativeScale();

	HitSquashAction.bActive = true;
	HitSquashAction.SquashInDuration = SquashInDuration > 0.0f ? SquashInDuration : 0.0f;
	HitSquashAction.RecoverDuration = RecoverDuration > 0.0f ? RecoverDuration : 0.0f;
	HitSquashAction.ElapsedTime = 0.0f;
	HitSquashAction.StartScale = OriginalScale;
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

	// SimulatePhysics 바디 분기 — PhysX 가 매 프레임 root transform 의 authority 라
	// AddActorWorldOffset 으로 더한 step 은 post-sync 가 덮어쓴다. step 이 1m teleport 임계값을
	// 넘는 FPS 에서만 우연히 fire → FPS-의존 (저FPS = 더 잘 날아감). 물리 바디한텐 PhysX 측
	// linear velocity 를 한 번에 박고 그 뒤는 PhysX 가 적분하도록 위임 — FPS 완전 독립.
	//
	// Distance 의 의미: 비물리 액터한텐 "Duration 동안 정확히 이만큼 이동" 이지만, 물리 바디는
	// 마찰/중력으로 어차피 exact 거리 보장 불가 → "초기 속도 (m/s)" 로 재해석한다. Distance/Duration
	// 을 그대로 쓰면 (예: 5/0.25 = 20 m/s) PhysX 마찰까지 고려해도 한참 멀리 날아가 의도보다
	// 훨씬 강하게 느껴짐. Distance 를 직접 속도로 쓰면 기본값 5 = 5 m/s 가 되어 마찰 있는 바닥에서
	// 몇 미터 굴러가는 자연스러운 거동.
	AActor* OwnerActor = GetOwner();
	UPrimitiveComponent* RootPrim = OwnerActor ? Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()) : nullptr;
	if (RootPrim && RootPrim->GetSimulatePhysics())
	{
		const FVector Velocity = KnockbackDirection * Distance;
		RootPrim->SetLinearVelocity(Velocity);

		// per-frame step tracking 은 끔 — PhysX 가 알아서 적분.
		(void)Duration; // 물리 바디한텐 의미 없음 (마찰이 감속 담당).
		KnockbackAction = FKnockbackAction();
		return;
	}

	// 비물리 액터 — 기존 per-frame AddActorWorldOffset step 경로.
	KnockbackAction.bActive = true;
	KnockbackAction.Duration = Duration > 0.0f ? Duration : 0.0f;
	KnockbackAction.RemainingTime = KnockbackAction.Duration;
	KnockbackAction.RemainingOffset = KnockbackDirection * Distance;

	if (KnockbackAction.Duration <= 0.0f)
	{
		if (OwnerActor)
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

	SlomoAction.bActive = true;
	SlomoAction.Duration = Duration;
	SlomoAction.RemainingTime = Duration;
	SlomoAction.TimeDilation = FMath::Clamp(TimeDilation, 0.0f, 1.0f);
	UpdateTimeDilationRegistration();
}

void UActionComponent::StopHitStop()
{
	HitStopAction = FTimedDilationAction();
	UpdateTimeDilationRegistration();
}

void UActionComponent::StopLocalHitStop()
{
	if (!LocalHitStopAction.bActive)
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->PrimaryActorTick.SetTickEnabled(LocalHitStopAction.bActorTickWasEnabled);
	}

	for (const TPair<UActorComponent*, bool>& State : LocalHitStopAction.ComponentTickStates)
	{
		UActorComponent* Component = State.first;
		if (IsAliveObject(Component))
		{
			Component->SetComponentTickEnabled(State.second);
		}
	}

	LocalHitStopAction = FLocalHitStopAction();
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
	UpdateTimeDilationRegistration();
}

void UActionComponent::StopAllActions()
{
	StopLocalHitStop();
	StopHitSquash();
	StopKnockback();
	HitStopAction = FTimedDilationAction();
	SlomoAction = FTimedDilationAction();
	UnregisterTimeDilationComponent();
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

void UActionComponent::UpdateTimeDilationRegistration()
{
	if (HasActiveTimeDilation())
	{
		RegisterTimeDilationComponent();
		return;
	}

	UnregisterTimeDilationComponent();
}

void UActionComponent::RegisterTimeDilationComponent()
{
	if (!GEngine || !GEngine->GetTimer())
	{
		return;
	}

	if (!bHasCapturedGlobalBaseTimeDilation)
	{
		GlobalBaseTimeDilation = GEngine->GetTimer()->GetTimeDilation();
		bHasCapturedGlobalBaseTimeDilation = true;
	}

	if (std::find(TimeDilationComponents.begin(), TimeDilationComponents.end(), this) == TimeDilationComponents.end())
	{
		TimeDilationComponents.push_back(this);
	}

	RefreshGlobalTimeDilation();
}

void UActionComponent::UnregisterTimeDilationComponent()
{
	auto It = std::find(TimeDilationComponents.begin(), TimeDilationComponents.end(), this);
	if (It != TimeDilationComponents.end())
	{
		TimeDilationComponents.erase(It);
	}

	RefreshGlobalTimeDilation();
}

void UActionComponent::RefreshGlobalTimeDilation()
{
	if (!GEngine || !GEngine->GetTimer())
	{
		return;
	}

	float SelectedDilation = 1.0f;
	bool bHasHitStop = false;
	bool bHasSlomo = false;

	auto It = TimeDilationComponents.begin();
	while (It != TimeDilationComponents.end())
	{
		UActionComponent* Component = *It;
		if (!IsAliveObject(Component) || !Component->HasActiveTimeDilation())
		{
			It = TimeDilationComponents.erase(It);
			continue;
		}

		if (Component->HasActiveTimeDilationAction(Component->HitStopAction))
		{
			SelectedDilation = bHasHitStop
				? (std::min)(SelectedDilation, Component->HitStopAction.TimeDilation)
				: Component->HitStopAction.TimeDilation;
			bHasHitStop = true;
		}

		++It;
	}

	if (!bHasHitStop)
	{
		for (UActionComponent* Component : TimeDilationComponents)
		{
			if (!Component || !Component->HasActiveTimeDilationAction(Component->SlomoAction))
			{
				continue;
			}

			SelectedDilation = bHasSlomo
				? (std::min)(SelectedDilation, Component->SlomoAction.TimeDilation)
				: Component->SlomoAction.TimeDilation;
			bHasSlomo = true;
		}
	}

	if (bHasHitStop || bHasSlomo)
	{
		GEngine->GetTimer()->SetTimeDilation(SelectedDilation);
		return;
	}

	TimeDilationComponents.clear();
	if (bHasCapturedGlobalBaseTimeDilation)
	{
		GEngine->GetTimer()->SetTimeDilation(GlobalBaseTimeDilation);
		bHasCapturedGlobalBaseTimeDilation = false;
	}
}

bool UActionComponent::HasActiveTimeDilationAction(const FTimedDilationAction& Action) const
{
	return Action.bActive && Action.RemainingTime > 0.0f;
}

bool UActionComponent::HasActiveTimeDilation() const
{
	return HasActiveTimeDilationAction(HitStopAction) || HasActiveTimeDilationAction(SlomoAction);
}
