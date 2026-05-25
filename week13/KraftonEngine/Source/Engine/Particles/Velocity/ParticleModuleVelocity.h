#pragma once
#include "ParticleModuleVelocityBase.h"

#include "Distributions/DistributionVector.h"
#include "Distributions/DistributionFloat.h"

#include "Source/Engine/Particles/Velocity/ParticleModuleVelocity.generated.h"

UCLASS()
class UParticleModuleVelocity : public UParticleModuleVelocityBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Velocity)
	struct FRawDistributionVector StartVelocity;

	/** 
	 *	The velocity to apply to a particle along its radial direction.
	 *	Direction is determined by subtracting the location of the emitter from the particle location at spawn.
	 *	Value is retrieved using the EmitterTime of the emitter.
	 */
	UPROPERTY(EditAnywhere, Category = Velocity)
	struct FRawDistributionFloat StartVelocityRadial;

	virtual void Spawn(const FSpawnContext& Context) override;

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};