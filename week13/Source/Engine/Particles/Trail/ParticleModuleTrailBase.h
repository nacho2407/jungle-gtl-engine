#pragma once

#include "Particles/ParticleModule.h"

UENUM()
enum EParticleSourceSelectionMethod : int
{
	EPSSM_Random,
	EPSSM_Sequential,
	EPSSM_MAX,
};

#include "Source/Engine/Particles/Trail/ParticleModuleTrailBase.generated.h"

UCLASS()
class UParticleModuleTrailBase : public UParticleModule
{
public:
	GENERATED_BODY()

	UParticleModuleTrailBase()
	{
		bSpawnModule = false;
		bUpdateModule = false;
	}

	EModuleType GetModuleType() const override { return EPMT_Trail; }
};
