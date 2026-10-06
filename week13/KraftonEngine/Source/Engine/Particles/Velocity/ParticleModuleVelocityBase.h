#pragma once
#include "Particles/ParticleModule.h"

#include "Source/Engine/Particles/Velocity/ParticleModuleVelocityBase.generated.h"

UCLASS()
class UParticleModuleVelocityBase : public UParticleModule
{
public:
	GENERATED_BODY()

	uint32 bInWorldSpace : 1;

	/** If true, then apply the particle system components scale to the velocity value. */
	uint32 bApplyOwnerScale : 1;
};
