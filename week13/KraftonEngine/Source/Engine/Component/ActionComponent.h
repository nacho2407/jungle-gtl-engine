#pragma once

#include "Component/ActorComponent.h"
#include "Math/Vector.h"

class USceneComponent;

class UActionComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UActionComponent, UActorComponent)

	UActionComponent() = default;
	~UActionComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	void HitStop(float Duration, float TimeDilation);
	void HitSquash(const FVector& SquashedScale, float SquashInDuration, float RecoverDuration);
	void Knockback(const FVector& Direction, float Distance, float Duration);
	void Slomo(float Duration, float TimeDilation);

	void StopHitStop();
	void StopHitSquash();
	void StopKnockback();
	void StopSlomo();
	void StopAllActions();

private:
	float GetRawDeltaTime(float FallbackDeltaTime) const;
	USceneComponent* GetTargetSceneComponent() const;
	void CaptureBaseTimeDilation();
	void ApplyTimeDilation();
	void ClearTimeDilationIfIdle();

	struct FTimedDilationAction
	{
		bool bActive = false;
		float Duration = 0.0f;
		float RemainingTime = 0.0f;
		float TimeDilation = 1.0f;
	};

	struct FHitSquashAction
	{
		bool bActive = false;
		float SquashInDuration = 0.0f;
		float RecoverDuration = 0.0f;
		float ElapsedTime = 0.0f;
		FVector StartScale = FVector::OneVector;
		FVector SquashedScale = FVector::OneVector;
	};

	struct FKnockbackAction
	{
		bool bActive = false;
		float Duration = 0.0f;
		float RemainingTime = 0.0f;
		FVector RemainingOffset = FVector::ZeroVector;
	};

	bool HasActiveTimeDilationAction(const FTimedDilationAction& Action) const;

	FTimedDilationAction HitStopAction;
	FTimedDilationAction SlomoAction;
	FHitSquashAction HitSquashAction;
	FKnockbackAction KnockbackAction;

	bool bHasCapturedBaseTimeDilation = false;
	float BaseTimeDilation = 1.0f;
};
