#pragma once

#include "Particles/ParticleModule.h"

UENUM()
enum Beam2SourceTargetMethod : int
{
	PEB2STM_Default,
	PEB2STM_UserSet,
	PEB2STM_Emitter,
	PEB2STM_Particle,
	PEB2STM_Actor,
	PEB2STM_MAX,
};

UENUM()
enum Beam2SourceTargetTangentMethod : int
{
	PEB2STTM_Direct,
	PEB2STTM_UserSet,
	PEB2STTM_Distribution,
	PEB2STTM_Emitter,
	PEB2STTM_MAX,
};

#include "Source/Engine/Particles/Beam/ParticleModuleBeamBase.generated.h"

UCLASS()
class UParticleModuleBeamBase : public UParticleModule
{
public:
	GENERATED_BODY()

	UParticleModuleBeamBase()
	{
		bSpawnModule = true;
		bUpdateModule = true;
	}

	EModuleType GetModuleType() const override { return EPMT_Beam; }
};
