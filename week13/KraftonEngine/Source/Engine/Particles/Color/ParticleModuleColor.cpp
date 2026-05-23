#include "ParticleModuleColor.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"

void UParticleModuleColor::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	Particle.BaseColor.R = StartColor.R / 255.0f;
	Particle.BaseColor.G = StartColor.G / 255.0f;
	Particle.BaseColor.B = StartColor.B / 255.0f;
	Particle.BaseColor.A = StartAlpha;
	Particle.Color = Particle.BaseColor;
}

#if WITH_EDITOR
void UParticleModuleColor::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
