#include "ParticleModuleLifetime.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Math/MathUtils.h"
#include <cstdlib>

void UParticleModuleLifetime::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	float Alpha = (float)rand() / (float)RAND_MAX;
	float Lifetime = FMath::Lerp(LifetimeMin, LifetimeMax, Alpha);
	Particle.OneOverMaxLifetime = (Lifetime > 0.0f) ? (1.0f / Lifetime) : 0.0f;
	Particle.RelativeTime = 0.0f;
}

float UParticleModuleLifetime::GetMaxLifetime()
{
	return LifetimeMax;
}

float UParticleModuleLifetime::GetLifetimeValue(const FContext& Context, float InTime, UObject* Data)
{
	return LifetimeMax;
}

#if WITH_EDITOR
void UParticleModuleLifetime::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
