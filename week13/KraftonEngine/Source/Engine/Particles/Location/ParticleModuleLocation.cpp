#include "ParticleModuleLocation.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"

void UParticleModuleLocation::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	Particle.Location += StartLocation;
}

#if WITH_EDITOR
void UParticleModuleLocation::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
