#include "ParticleModuleVelocity.h"

void UParticleModuleVelocity::Spawn(const FSpawnContext& Context)
{
}

#if WITH_EDITOR
void UParticleModuleVelocity::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
