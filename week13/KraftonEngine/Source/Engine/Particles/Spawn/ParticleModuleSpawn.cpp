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
	return SpawnRate * SpawnRateScale;
}

float UParticleModuleSpawn::GetEstimatedSpawnRate()
{
	// 원래는 Distribution 데이터로 존재할 때 다른 계산 방식이 있는 거 같은데
	// 지금은 단순하게 float 데이터로 지정했기에 GetMaximumSpawnRate 과 반환값 같음
	return SpawnRate * SpawnRateScale;
}

int32 UParticleModuleSpawn::GetMaximumBurstCount()
{
	int32 MaxBurst = 0;
	return MaxBurst;
}

void UParticleModuleSpawn::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
