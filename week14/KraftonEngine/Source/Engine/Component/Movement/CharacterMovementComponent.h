#pragma once

#include "MovementComponent.h"
#include "Math/Vector.h"

// UE 의 UCharacterMovementComponent 를 minimal subset 으로 흉내 — 평면 위 walking 만.
//   - Controller 가 매 frame AddInputVector 로 입력 누적.
//   - TickComponent 가 입력 → acceleration → velocity (MaxWalkSpeed 클램프) → UpdatedComponent world offset.
//   - 입력 없으면 BrakingFriction 으로 감속.
//   - Z=0 평지 가정 — gravity/jump/floor sweep 은 후속 phase.
//
// Editor-set: MaxWalkSpeed / MaxAcceleration / BrakingFriction.
// 런타임 read: GetVelocity / GetSpeed — AnimInstance 가 Speed 기반 FSM 전이.
class UCharacterMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UCharacterMovementComponent, UMovementComponent)

	UCharacterMovementComponent() = default;
	~UCharacterMovementComponent() override = default;

	// Controller 등 외부에서 매 frame 누적. TickComponent 가 ConsumeInputVector 로 비움.
	void AddInputVector(const FVector& WorldDirection, float ScaleValue = 1.0f);
	void ConsumeInputVector(FVector& OutAccumulated);

	const FVector& GetVelocity() const { return Velocity; }
	float          GetSpeed()    const { return Velocity.Length(); }

	// UMovementComponent:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

	// Editor-set 파라미터.
	float MaxWalkSpeed    = 6.0f;     // m/s — Idle/Walk threshold 기준 정도
	float MaxAcceleration = 20.0f;    // m/s^2
	float BrakingFriction = 8.0f;     // 입력 없을 때 감속률 (m/s^2)

protected:
	FVector AccumulatedInput = FVector(0.0f, 0.0f, 0.0f);
	FVector Velocity         = FVector(0.0f, 0.0f, 0.0f);
};
