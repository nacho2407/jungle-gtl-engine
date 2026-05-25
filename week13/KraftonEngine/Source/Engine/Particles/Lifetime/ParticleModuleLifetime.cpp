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

	if (Particle.OneOverMaxLifetime > 0.f)
	{
		// 다른 모듈이 Lifetime 을 수정한 상태
		float CurrentLifetime = 1.f / Particle.OneOverMaxLifetime;
		Particle.OneOverMaxLifetime = 1.f / (Lifetime + CurrentLifetime);
	}
	else
	{
		Particle.OneOverMaxLifetime = (Lifetime > 0.0f) ? (1.0f / Lifetime) : 0.0f;
	}

	// 1.0f 보다 크다면 이미 다른 모듈이 해당 파티클을 죽인 상태라고 볼 수 있음
	Particle.RelativeTime = Particle.RelativeTime > 1.0f ? Particle.RelativeTime : Context.SpawnTime * Particle.OneOverMaxLifetime;
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

#include "Serialization/Archive.h"

void UParticleModuleLifetime::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	Ar << LifetimeMin;
	Ar << LifetimeMax;
}
