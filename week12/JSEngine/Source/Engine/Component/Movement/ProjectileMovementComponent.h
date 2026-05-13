#pragma once

#include "MovementComponent.h"

class UProjectileMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

	virtual void TickComponent(float DeltaTime) override;
	virtual void BeginPlay() override;

    void SetInitialSpeed(const float InSpeed) { InitialSpeed = InSpeed; }
    float GetInitialSpeed() const { return InitialSpeed; }

    void SetMaxSpeed(const float InSpeed) { MaxSpeed = InSpeed; }
	virtual float GetMaxSpeed() const { return MaxSpeed; }

    void SetGravityScale(const float InScale) { GravityScale = InScale; }
    float GetGravityScale() const { return GravityScale; }

    void SetRotationFollowsVelocity(bool bFollow) { bRotationFollowsVelocity = bFollow; }
    bool GetRotationFollowsVelocity() const { return bRotationFollowsVelocity; }

	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
	float InitialSpeed = 5.0f;
	float MaxSpeed = 100.0f;
	float GravityScale = 0.0f;

	bool bRotationFollowsVelocity = true; // 켤 시 화살 및 로켓이 날아가는 궤적을 바라본다.
};