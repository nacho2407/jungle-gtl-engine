#include "ParticleModuleSpawn.h"

bool UParticleModuleSpawn::GetSpawnAmount(const FSpawnContext& Context, int32 Offset, float OldLeftover, float DeltaTime, int32& OutNumber, float& OutRate)
{
	return false;
}

bool UParticleModuleSpawn::GetBurstCount(const FSpawnContext& Context, int32 Offset, float OldLeftover, float DeltaTime, int32& OutBurstCount)
{
	return false;
}

float UParticleModuleSpawn::GetMaximumSpawnRate()
{
	return 0.0f;
}

float UParticleModuleSpawn::GetEstimatedSpawnRate()
{
	return 0.0f;
}

int32 UParticleModuleSpawn::GetMaximumBurstCount()
{
	return 0;
}

void UParticleModuleSpawn::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	UParticleModuleSpawnBase::PostEditChangeProperty(PropertyChangedEvent);
}
