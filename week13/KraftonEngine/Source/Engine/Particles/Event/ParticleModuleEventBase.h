#pragma once
#include "Particles/ParticleModule.h"

#include "Source/Engine/Particles/Event/ParticleModuleEventBase.generated.h"

UCLASS()
class UParticleModuleEventBase : public UParticleModule
{
public:
	GENERATED_BODY()

	virtual EModuleType GetModuleType() const override { return EPMT_Event; }
};
