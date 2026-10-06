#pragma once

#include "Particles/Spawn/ParticleModuleSpawnBase.h"

#include "Source/Engine/Particles/Spawn/ParticleModuleSpawnPerUnit.generated.h"

UCLASS()
class UParticleModuleSpawnPerUnit : public UParticleModuleSpawnBase
{
public:
	GENERATED_BODY()

	float UnitScalar = 1.0f;
	float MovementTolerance = 0.1f;
	float SpawnPerUnit = 0.0f;
	float MaxFrameDistance = 0.0f;
	uint32 bIgnoreSpawnRateWhenMoving : 1;
	uint32 bIgnoreMovementAlongX : 1;
	uint32 bIgnoreMovementAlongY : 1;
	uint32 bIgnoreMovementAlongZ : 1;

	UParticleModuleSpawnPerUnit();
	bool GetSpawnAmount(const FSpawnContext& Context, int32 Offset, float OldLeftover, float DeltaTime, int32& OutNumber, float& OutRate) override;
	void Serialize(FArchive& Ar) override;
};
