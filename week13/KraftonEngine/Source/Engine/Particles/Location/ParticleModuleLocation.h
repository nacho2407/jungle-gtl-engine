#pragma once
#include "ParticleModuleLocationBase.h"

#include "Source/Engine/Particles/Location/ParticleModuleLocation.generated.h"

UCLASS()
class UParticleModuleLocation : public UParticleModuleLocationBase
{
public:
	GENERATED_BODY()

	/**
	 *	The location the particle should be emitted.
	 *	Relative in local space to the emitter by default.
	 *	Relative in world space as a WorldOffset module or when the emitter's UseLocalSpace is off.
	 *	Retrieved using the EmitterTime at the spawn of the particle.
	 */
	UPROPERTY(EditAnywhere, Category = "Location")
	FVector StartLocation;

	/*
	Spawn
		Particle->Position = RandomPointInShape();
	*/
	virtual void Spawn(const FSpawnContext& Context) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
