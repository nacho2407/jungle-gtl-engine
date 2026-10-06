#pragma once
#include "ParticleModuleEventBase.h"

#include "Source/Engine/Particles/Event/ParticleModuleEventGenerator.generated.h"

UCLASS()
class UParticleModuleEventGenerator : public UParticleModuleEventBase
{
public:
	GENERATED_BODY()

	bool bGenerateCollisionEvents = false;
	bool bGenerateDeathEvents = false;
	bool bGenerateSpawnEvents = false;

	virtual void Serialize(FArchive& Ar) override;
};
