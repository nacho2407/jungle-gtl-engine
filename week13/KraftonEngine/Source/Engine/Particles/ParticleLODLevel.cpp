#include "Particles/ParticleLODLevel.h"

#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Color/ParticleModuleColor.h"

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

	// 각 모듈이 자신의 bSpawnModule/bUpdateModule 플래그로 어느 단계에 들어갈지 결정한다.
	// 같은 모듈이 두 리스트에 모두 들어갈 수 있다 (예: Velocity, Size, Color).
	for (UParticleModule* Module : Modules)
	{
		if (!Module)
		{
			continue;
		}

		if (Module->bSpawnModule)
		{
			SpawnModules.push_back(Module);
		}
		if (Module->bUpdateModule)
		{
			UpdateModules.push_back(Module);
		}
	}
}
