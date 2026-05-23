#pragma once
#include "ParticleModuleSpawnBase.h"
#include "Particles/ParticleEmitter.h"


#include "Source/Engine/Particles/Spawn/ParticleModuleSpawn.generated.h"

UCLASS()
class UParticleModuleSpawn : public UParticleModuleSpawnBase
{
public:
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnRate = 10.0f;
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnRateScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Burst")
	TArray<FParticleBurst> BurstList;
	UPROPERTY(EditAnywhere, Category = "Burst")
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
