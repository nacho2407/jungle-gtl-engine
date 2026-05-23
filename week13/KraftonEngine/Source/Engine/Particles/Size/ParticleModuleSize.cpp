#include "ParticleModuleSize.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"

void UParticleModuleSize::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	Particle.BaseSize = StartSize;
	Particle.Size = StartSize;
}

void UParticleModuleSize::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
