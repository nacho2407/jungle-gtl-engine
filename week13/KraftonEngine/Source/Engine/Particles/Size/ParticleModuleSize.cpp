#include "ParticleModuleSize.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"

void UParticleModuleSize::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	Particle.BaseSize = StartSize;
	Particle.Size = StartSize;
}

#if WITH_EDITOR
void UParticleModuleSize::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#include "Serialization/Archive.h"

void UParticleModuleSize::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	Ar << StartSize;
}
