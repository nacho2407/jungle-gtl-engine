#pragma once
#include "ParticleModuleSpawnBase.h"

struct FParticleBurst;

class UParticleModuleSpawn : public UParticleModuleSpawnBase
{
public:
	float SpawnRate = 10.0f;
	float SpawnRateScale = 1.0f;

	TArray<FParticleBurst> BurstList;
	float BurstScale = 1.0f;

	virtual bool GetSpawnAmount(
		const FSpawnContext& Context,
		int32 Offset,
		float OldLeftover,
		float DeltaTime,
		int32& OutNumber,
		float& OutRate) override;

	virtual bool GetBurstCount(
		const FSpawnContext& Context,
		int32 Offset,
		float OldLeftover,
		float DeltaTime,
		int32& OutBurstCount) override;

	virtual float GetMaximumSpawnRate() override;
	virtual float GetEstimatedSpawnRate() override;
	virtual int32 GetMaximumBurstCount() override;

#if WITH_EDITOR
	virtual void	PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};