#pragma once

#include "Distributions/DistributionFloat.h"
#include "Object/FName.h"
#include "Particles/Trail/ParticleModuleTrailBase.h"

UENUM()
enum ETrail2SourceMethod : int
{
	PET2SRCM_Default,
	PET2SRCM_Particle,
	PET2SRCM_Actor,
	PET2SRCM_MAX,
};

#include "Source/Engine/Particles/Trail/ParticleModuleTrailSource.generated.h"

UCLASS()
class UParticleModuleTrailSource : public UParticleModuleTrailBase
{
public:
	GENERATED_BODY()

	ETrail2SourceMethod SourceMethod = PET2SRCM_Default;
	FName SourceName = FName::None;
	FRawDistributionFloat SourceStrength;
	uint32 bLockSourceStrength : 1;
	int32 SourceOffsetCount = 0;
	TArray<FVector> SourceOffsetDefaults;
	EParticleSourceSelectionMethod SelectionMethod = EPSSM_Random;
	uint32 bInheritRotation : 1;

	UParticleModuleTrailSource();
	void InitializeDefaults();
	void Serialize(FArchive& Ar) override;

	bool ResolveSourceOffset(int32 InTrailIdx, FParticleEmitterInstance* InEmitterInst, FVector& OutSourceOffset);
};
