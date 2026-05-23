#include "ParticleModuleLifetime.h"

void UParticleModuleLifetime::Spawn(const FSpawnContext& Context)
{
}

float UParticleModuleLifetime::GetMaxLifetime()
{
	return 0.0f;
}

float UParticleModuleLifetime::GetLifetimeValue(const FContext& Context, float InTime, UObject* Data)
{
	return 0.0f;
}

#if WITH_EDITOR
void UParticleModuleLifetime::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
