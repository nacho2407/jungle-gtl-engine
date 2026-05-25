#include "ParticleModuleSize.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"

void UParticleModuleSize::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	FVector Size = StartSize;

	Particle.Size += Size;
	// Flip 을 Scale 로 표현. 
	// AdjustParticleBaseSizeForUVFlipping(Size, Context.Owner.GetCurrentLODLevelChecked()->RequiredModule.UVFlippingMode, *InRandomStream);
	Particle.BaseSize += Size;
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
