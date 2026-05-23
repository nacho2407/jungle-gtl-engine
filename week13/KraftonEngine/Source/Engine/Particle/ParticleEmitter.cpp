#include "Particle/ParticleEmitter.h"

#include "Particles/ParticleModule.h"
#include "Serialization/Archive.h"

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODIndex) const
{
	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevels.size()))
	{
		return nullptr;
	}

	return LODLevels[LODIndex];
}

void UParticleEmitter::AddModuleOffsetToAllLODs(int32 ModuleIndex, uint32 Offset)
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (!LODLevel || ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(LODLevel->Modules.size()))
		{
			continue;
		}
		if (LODLevel->Modules[ModuleIndex])
		{
			ModuleOffsetMap[LODLevel->Modules[ModuleIndex]] = Offset;
		}
	}
}

void UParticleEmitter::AddModuleInstanceOffsetToAllLODs(int32 ModuleIndex, uint32 Offset)
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (!LODLevel || ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(LODLevel->Modules.size()))
		{
			continue;
		}
		if (LODLevel->Modules[ModuleIndex])
		{
			ModuleInstanceOffsetMap[LODLevel->Modules[ModuleIndex]] = Offset;
		}
	}
}

void UParticleEmitter::AddModuleRandomSeedOffsetToAllLODs(int32 ModuleIndex, uint32 Offset)
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (!LODLevel || ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(LODLevel->Modules.size()))
		{
			continue;
		}
		if (LODLevel->Modules[ModuleIndex])
		{
			ModuleRandomSeedInstanceOffsetMap[LODLevel->Modules[ModuleIndex]] = Offset;
		}
	}
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	ModuleOffsetMap.clear();
	ModuleInstanceOffsetMap.clear();
	ModuleRandomSeedInstanceOffsetMap.clear();

	DynamicParameterDataOffset = 0;
	LightDataOffset = 0;
	CameraPayloadOffset = 0;
	OrbitModuleOffset = 0;

	ParticleSize = sizeof(FBaseParticle);
	ReqInstanceBytes = 0;

	TypeDataOffset = 0;
	TypeDataInstanceOffset = -1;

	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->UpdateModuleLists();
		}
	}

	UParticleLODLevel* HighLODLevel = GetLODLevel(0);
	if (!HighLODLevel)
	{
		return;
	}

	UParticleModuleTypeDataBase* HighTypeData = HighLODLevel->TypeDataModule;

	if (HighTypeData)
	{
		const int32 ReqBytes = static_cast<int32>(HighTypeData->RequiredBytes(nullptr));
		if (ReqBytes > 0)
		{
			TypeDataOffset = ParticleSize;
			ParticleSize += ReqBytes;
		}

		const int32 InstanceBytes = static_cast<int32>(HighTypeData->RequiredBytesPerInstance());
		if (InstanceBytes > 0)
		{
			TypeDataInstanceOffset = ReqInstanceBytes;
			ReqInstanceBytes += InstanceBytes;
		}
	}

	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(HighLODLevel->Modules.size()); ++ModuleIndex)
	{
		UParticleModule* ParticleModule = HighLODLevel->Modules[ModuleIndex];
		if (!ParticleModule)
		{
			continue;
		}

		const int32 ReqBytes = static_cast<int32>(ParticleModule->RequiredBytes(HighTypeData));
		if (ReqBytes > 0)
		{
			AddModuleOffsetToAllLODs(ModuleIndex, static_cast<uint32>(ParticleSize));

			if (Cast<UParticleModuleCameraOffset>(ParticleModule))
			{
				CameraPayloadOffset = ParticleSize;
			}
			if (Cast<UParticleModuleOrbit>(ParticleModule))
			{
				OrbitModuleOffset = ParticleSize;
			}
			if (Cast<UParticleModuleLight>(ParticleModule))
			{
				LightDataOffset = ParticleSize;
			}

			ParticleSize += ReqBytes;
		}

		const int32 InstanceBytes = static_cast<int32>(ParticleModule->RequiredBytesPerInstance());
		if (InstanceBytes > 0)
		{
			AddModuleInstanceOffsetToAllLODs(ModuleIndex, static_cast<uint32>(ReqInstanceBytes));
			ReqInstanceBytes += InstanceBytes;
		}

		if (ParticleModule->RequiresRandomSeedInstancePayload())
		{
			AddModuleRandomSeedOffsetToAllLODs(ModuleIndex, static_cast<uint32>(ReqInstanceBytes));
			ReqInstanceBytes += sizeof(FParticleRandomSeedInstancePayload);
		}
	}
}

void UParticleEmitter::Serialize(FArchive& Ar)
{
	int32 Version = 0;
	Ar << Version;
	Ar << bEnabled;
}
