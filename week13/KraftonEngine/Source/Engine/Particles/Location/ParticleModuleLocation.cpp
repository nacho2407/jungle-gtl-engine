#include "ParticleModuleLocation.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Core/Logging/Log.h"

void UParticleModuleLocation::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	FVector LocationOffset;

	LocationOffset = StartLocation.GetValue(Context.Owner.EmitterTime);
	LocationOffset = Context.Owner.EmitterToSimulation.TransformVector(LocationOffset);
	Particle.Location += LocationOffset;

	if (Particle.Location.ContainsNaN())
	{
		UE_LOG("NaN in Particle Location. Template: %s, Component: %s", Context.GetTemplateName().c_str(), Context.GetInstanceName().c_str());
	}
}

#if WITH_EDITOR
void UParticleModuleLocation::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#include "Serialization/Archive.h"

void UParticleModuleLocation::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	StartLocation.Serialize(Ar);
}
