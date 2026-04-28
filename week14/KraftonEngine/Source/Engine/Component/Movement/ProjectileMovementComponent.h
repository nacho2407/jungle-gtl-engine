#pragma once

#include "Component/Movement/MovementComponent.h"
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
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;
	void ContributeSelectedVisuals(FScene& Scene) const override;

	void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }
	const FVector& GetVelocity() const { return Velocity; }
	void SetInitialSpeed(float InInitialSpeed) { InitialSpeed = InInitialSpeed; }
	float GetInitialSpeed() const { return InitialSpeed; }
	float GetMaxSpeed() const { return MaxSpeed; }
	FVector GetPreviewVelocity() const;
	void StopSimulating();

protected:
	FVector ComputeEffectiveVelocity() const;
	virtual EProjectileHitBehavior GetHitBehavior() const;
	virtual bool HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult);

	FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
	float InitialSpeed = 10.0f;
	float MaxSpeed = 100.0f;
};
