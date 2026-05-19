#pragma once

#include "ProjectileMovementComponent.generated.h"

#include "MovementComponent.h"

UCLASS()
class UProjectileMovementComponent : public UMovementComponent
{
    GENERATED_BODY_UProjectileMovementComponent()
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
	UPROPERTY(EditAnywhere, Category="Movement", DisplayName="Initial Speed")
	float InitialSpeed = 5.0f;
	UPROPERTY(EditAnywhere, Category="Movement", DisplayName="Max Speed")
	float MaxSpeed = 100.0f;
	UPROPERTY(EditAnywhere, Category="Movement", DisplayName="Gravity Scale")
	float GravityScale = 0.0f;

	UPROPERTY(EditAnywhere, Category="Rotation", DisplayName="Rotation Follows Velocity")
	bool bRotationFollowsVelocity = true; // 켤 시 화살 및 로켓이 날아가는 궤적을 바라본다.
};
