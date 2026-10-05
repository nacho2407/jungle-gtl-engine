#pragma once

#include "MovementComponent.h"

UCLASS(SpawnableComponent, DisplayName = "ProjectileMovement Component", Category = "Movement")
class UProjectileMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY(UProjectileMovementComponent, UMovementComponent)

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

private:
	UPROPERTY(DisplayName = "Initial Speed", Min = 0.0f, Speed = 1.0f, LuaReadWrite, LuaName = InitialSpeed)
	float InitialSpeed = 5.0f;

	UPROPERTY(DisplayName = "Max Speed", Min = 0.0f, Speed = 1.0f, LuaReadWrite, LuaName = MaxSpeed)
	float MaxSpeed = 100.0f;

	UPROPERTY(DisplayName = "Gravity Scale", Min = 0.0f, Max = 5.0f, Speed = 0.01f, LuaReadWrite, LuaName = GravityScale)
	float GravityScale = 0.0f;

	UPROPERTY(DisplayName = "Rotation Follows Velocity", LuaReadWrite, LuaName = RotationFollowsVelocity)
	bool bRotationFollowsVelocity = true; // 켤 시 화살 및 로켓이 날아가는 궤적을 바라본다.
};
