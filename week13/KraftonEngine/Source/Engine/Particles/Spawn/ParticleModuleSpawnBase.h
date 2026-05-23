#pragma once
#include "Particles/ParticleModule.h"

#include "Source/Engine/Particles/Spawn/ParticleModuleSpawnBase.generated.h"

UCLASS()
class UParticleModuleSpawnBase : public UParticleModule
{
public:
	GENERATED_BODY()

	uint8 bProcessSpawnRate : 1;
	uint8 bProcessBurstList : 1;

	virtual EModuleType	GetModuleType() const override { return EPMT_Spawn; }

	virtual bool GetSpawnAmount(
		const FSpawnContext& Context,
		int32 Offset,
		float OldLeftover,
		float DeltaTime,
		int32& OutNumber,
		float& OutRate)
	{
		return bProcessSpawnRate;
	}

	virtual bool GetBurstCount(
		const FSpawnContext& Context,
		int32 Offset,
		float OldLeftover,
		float DeltaTime,
		int32& OutBurstCount)
	{
		OutBurstCount = 0;
		return bProcessBurstList;
	}

	virtual float GetMaximumSpawnRate() { return 0.0f; }
	virtual float GetEstimatedSpawnRate() { return 0.0f; }
	virtual int32 GetMaximumBurstCount() { return 0; }
};
