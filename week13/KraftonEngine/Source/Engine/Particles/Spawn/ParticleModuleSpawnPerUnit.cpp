#include "Particles/Spawn/ParticleModuleSpawnPerUnit.h"

#include "Serialization/Archive.h"

UParticleModuleSpawnPerUnit::UParticleModuleSpawnPerUnit()
	: bIgnoreSpawnRateWhenMoving(false)
	, bIgnoreMovementAlongX(false)
	, bIgnoreMovementAlongY(false)
	, bIgnoreMovementAlongZ(false)
{
	bSpawnModule = false;
	bUpdateModule = false;
	MovementTolerance = 0.1f;
}

bool UParticleModuleSpawnPerUnit::GetSpawnAmount(const FSpawnContext& Context, int32 Offset, float OldLeftover, float DeltaTime, int32& OutNumber, float& OutRate)
{
	// UE original responsibility: convert source movement distance plus leftover distance into
	// spawn count/rate.
	// Missing Jungle foundation: ribbon Spawn_Source owns source-distance slicing and calls
	// FParticleRibbonEmitterInstance::GetSpawnPerUnitAmount directly.
	// System to connect later: full ParticleTrail2EmitterInstance SpawnPerUnit module path.
	OutNumber = 0;
	OutRate = 0.0f;
	return false;
}

void UParticleModuleSpawnPerUnit::Serialize(FArchive& Ar)
{
	UParticleModuleSpawnBase::Serialize(Ar);
	Ar << UnitScalar << MovementTolerance << SpawnPerUnit << MaxFrameDistance;
	bool IgnoreSpawnRateWhenMoving = bIgnoreSpawnRateWhenMoving;
	bool IgnoreMovementAlongX = bIgnoreMovementAlongX;
	bool IgnoreMovementAlongY = bIgnoreMovementAlongY;
	bool IgnoreMovementAlongZ = bIgnoreMovementAlongZ;
	Ar << IgnoreSpawnRateWhenMoving << IgnoreMovementAlongX << IgnoreMovementAlongY << IgnoreMovementAlongZ;
	if (Ar.IsLoading())
	{
		bIgnoreSpawnRateWhenMoving = IgnoreSpawnRateWhenMoving;
		bIgnoreMovementAlongX = IgnoreMovementAlongX;
		bIgnoreMovementAlongY = IgnoreMovementAlongY;
		bIgnoreMovementAlongZ = IgnoreMovementAlongZ;
	}
}
