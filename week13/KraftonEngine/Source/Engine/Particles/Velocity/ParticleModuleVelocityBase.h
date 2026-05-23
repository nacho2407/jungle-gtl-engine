#pragma once
#include "Particles/ParticleModule.h"

class UParticleModuleVelocityBase : public UParticleModule
{
public:
	uint32 bInWorldSpace : 1;

	/** If true, then apply the particle system components scale to the velocity value. */
	uint32 bApplyOwnerScale : 1;
};
