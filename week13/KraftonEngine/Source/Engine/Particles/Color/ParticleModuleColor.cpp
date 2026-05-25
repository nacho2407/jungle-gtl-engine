#include "Object/GarbageCollection.h"
#include "ParticleModuleColor.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"

void UParticleModuleColor::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	FVector Color = StartColor.GetValue(Context.Owner.EmitterTime);
	float Alpha = StartAlpha.GetValue(Context.Owner.EmitterTime);

	Particle.BaseColor.R = Color.R;
	Particle.BaseColor.G = Color.G;
	Particle.BaseColor.B = Color.B;
	Particle.BaseColor.A = Alpha;
	Particle.Color = Particle.BaseColor;
}

#if WITH_EDITOR
void UParticleModuleColor::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#include "Serialization/Archive.h"

void UParticleModuleColor::AddReferencedObjects(FReferenceCollector& Collector)
{
	UParticleModuleColorBase::AddReferencedObjects(Collector);
	StartColor.AddReferencedObjects(Collector);
	StartAlpha.AddReferencedObjects(Collector);
}

void UParticleModuleColor::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	// FColor는 trivially copyable (uint32 R,G,B,A) — generic template으로 한 번에 쓰면
	// 엔디안 문제만 없으면 안전하다.
	StartColor.Serialize(Ar);
	StartAlpha.Serialize(Ar);

	bool bClamp = bClampAlpha;
	Ar << bClamp;
	if (Ar.IsLoading())
	{
		bClampAlpha = bClamp ? 1 : 0;
	}
}
