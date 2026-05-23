#include "ParticleModuleLocation.h"

void UParticleModuleLocation::Spawn(const FSpawnContext& Context)
{
}

#if WITH_EDITOR
void UParticleModuleLocation::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
