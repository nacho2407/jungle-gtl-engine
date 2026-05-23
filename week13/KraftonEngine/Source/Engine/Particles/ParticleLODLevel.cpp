#include "Particles/ParticleLODLevel.h"

#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"

#include <algorithm>

int32 UParticleLODLevel::CalculateMaxActiveParticleCount() const
{
	int32 Estimate = 32;

	if (SpawnModule)
	{
		Estimate += static_cast<int32>(SpawnModule->SpawnRate * 2.0f);
		Estimate += SpawnModule->GetMaximumBurstCount();
	}

	if (RequiredModule)
	{
		const float Duration = std::max(0.001f, RequiredModule->EmitterDuration);
		if (SpawnModule)
		{
			Estimate += static_cast<int32>(SpawnModule->SpawnRate * SpawnModule->SpawnRateScale * Duration);
		}
	}

	return std::max(Estimate, 32);
}

void UParticleLODLevel::UpdateModuleLists()
{
	SpawnModules.clear();
	UpdateModules.clear();
	OrbitModules.clear();

	if (SpawnModule)
	{
		SpawnModules.push_back(SpawnModule);
	}

	for (UParticleModule* Module : Modules)
	{
		if (!Module)
		{
			continue;
		}

		if (Cast<UParticleModuleLifetime>(Module) ||
			Cast<UParticleModuleLocation>(Module) ||
			Cast<UParticleModuleVelocity>(Module) ||
			Cast<UParticleModuleColor>(Module) ||
			Cast<UParticleModuleSize>(Module) ||
			Cast<UParticleModuleCameraOffset>(Module) ||
			Cast<UParticleModuleOrbit>(Module))
		{
			SpawnModules.push_back(Module);
		}

		if (Cast<UParticleModuleSizeScaleBySpeed>(Module))
		{
			UpdateModules.push_back(Module);
		}

		if (Cast<UParticleModuleOrbit>(Module))
		{
			OrbitModules.push_back(Module);
		}
	}
}
