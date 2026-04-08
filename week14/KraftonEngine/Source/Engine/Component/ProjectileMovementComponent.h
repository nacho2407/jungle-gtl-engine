#pragma once

#include "Component/MovementComponent.h"
#include "Core/CollisionTypes.h"
#include "Core/CoreTypes.h"
#include "Math/Vector.h"

enum class EProjectileHitBehavior : int32
{
	Stop = 0,
	Bounce = 1,
	Destroy = 2,
};

class UProjectileMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

	UProjectileMovementComponent() = default;
	~UProjectileMovementComponent() override = default;

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

	void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }
	void SetInitialSpeed(float InInitialSpeed) { InitialSpeed = InInitialSpeed; }
	void StopSimulating();

protected:
	virtual EProjectileHitBehavior GetHitBehavior() const;
	virtual bool HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult);

	FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
	FVector Direction = FVector(1.0f, 0.0f, 0.0f);
	float InitialSpeed = 1200.0f;
	float MaxSpeed = 3000.0f;
};
