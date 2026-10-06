#include "ParticleModuleEventGenerator.h"
#include "Serialization/Archive.h"

void UParticleModuleEventGenerator::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << bGenerateCollisionEvents;
	Ar << bGenerateDeathEvents;
	Ar << bGenerateSpawnEvents;

}
