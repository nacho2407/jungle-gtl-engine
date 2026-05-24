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

#include "Serialization/Archive.h"

void UParticleModuleColor::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	// FColor는 trivially copyable (uint32 R,G,B,A) — generic template으로 한 번에 쓰면
	// 엔디안 문제만 없으면 안전하다.
	Ar << StartColor;
	Ar << StartAlpha;

	bool bClamp = bClampAlpha;
	Ar << bClamp;
	if (Ar.IsLoading())
	{
		bClampAlpha = bClamp ? 1 : 0;
	}
}
