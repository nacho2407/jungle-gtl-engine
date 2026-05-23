#pragma once
#include "ParticleModuleLocationBase.h"

class UParticleModuleLocation : public UParticleModuleLocationBase
{
public:
	/**
	 *	The location the particle should be emitted.
	 *	Relative in local space to the emitter by default.
	 *	Relative in world space as a WorldOffset module or when the emitter's UseLocalSpace is off.
	 *	Retrieved using the EmitterTime at the spawn of the particle.
	 */
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
