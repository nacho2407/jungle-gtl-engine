#include "Particle/ParticleEmitterInstance.h"

#include "Particle/ParticleAsset.h"

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	(void)DeltaTime;

	if (CurrentLODLevel == nullptr || !CurrentLODLevel->bEnabled)
	{
		return;
	}

	for (UParticleModule* Module : CurrentLODLevel->Modules)
	{
		if (Module != nullptr)
		{
			Module->Update(this, 0, DeltaTime);
		}
	}
}
