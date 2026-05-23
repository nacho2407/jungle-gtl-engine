#include "Particles/ParticleEmitter.h"

#include "ParticleModuleRequired.h"
#include "Color/ParticleModuleColor.h"
#include "Lifetime/ParticleModuleLifetime.h"
#include "Location/ParticleModuleLocation.h"
#include "Particles/ParticleModule.h"
#include "Serialization/Archive.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Size/ParticleModuleSize.h"
#include "Spawn/ParticleModuleSpawn.h"
#include "Velocity/ParticleModuleVelocity.h"

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

			// TODO: Set CameraPayloadOffset and OrbitModuleOffset when
			// UParticleModuleCameraOffset and UParticleModuleOrbit are implemented.
			// TODO: Add payload handling when UParticleModuleColor,
			// UParticleModuleSize, and UParticleModuleSizeScaleBySpeed are implemented.
			//if (Cast<UParticleModuleLight>(ParticleModule))
			//{
			//	LightDataOffset = ParticleSize;
			//}

			ParticleSize += ReqBytes;
		}

		const int32 InstanceBytes = static_cast<int32>(ParticleModule->RequiredBytesPerInstance());
		if (InstanceBytes > 0)
		{
			AddModuleInstanceOffsetToAllLODs(ModuleIndex, static_cast<uint32>(ReqInstanceBytes));
			ReqInstanceBytes += InstanceBytes;
		}

		//if (ParticleModule->RequiresRandomSeedInstancePayload())
		//{
		//	AddModuleRandomSeedOffsetToAllLODs(ModuleIndex, static_cast<uint32>(ReqInstanceBytes));
		//	ReqInstanceBytes += sizeof(FParticleRandomSeedInstancePayload);
		//}
	}
}

void UParticleEmitter::Serialize(FArchive& Ar)
{
	int32 Version = 0;
	Ar << Version;

	bool bSerializedEnabled         = bEnabled;
	bool bSerializedUseMeshInstance = bUseMeshInstance;

	Ar << bSerializedEnabled;
	Ar << bSerializedUseMeshInstance;

	if (Ar.IsLoading())
	{
		bUseMeshInstance = bSerializedUseMeshInstance;

		if (!bUseMeshInstance)
		{
			InitializeDefaultSpriteEmitter();
		}

		SetEnabled(bSerializedEnabled);
	}
}

void UParticleEmitter::InitializeDefaultSpriteEmitter()
{
	if (HasValidLOD0())
	{
		CacheEmitterModuleInfo();
		return;
	}

	LODLevels.clear();

	bUseMeshInstance = false;
	bEnabled         = true;

	ParticleSize               = sizeof(FBaseParticle);
	ReqInstanceBytes           = 0;
	TypeDataOffset             = 0;
	TypeDataInstanceOffset     = -1;
	DynamicParameterDataOffset = 0;
	LightDataOffset            = 0;
	CameraPayloadOffset        = 0;
	OrbitModuleOffset          = 0;

	InitialAllocationCount     = 32;
	QualityLevelSpawnRateScale = 1.0f;
	PivotOffset                = FVector::ZeroVector;

	UParticleLODLevel* LOD = UObjectManager::Get().CreateObject<UParticleLODLevel>(this);

	LOD->Level               = 0;
	LOD->bEnabled            = true;
	LOD->PeakActiveParticles = 0;

	UParticleModuleRequired* Required = UObjectManager::Get().CreateObject<UParticleModuleRequired>(LOD);

	Required->bEnabled           = true;
	Required->bSpawnModule       = false;
	Required->bUpdateModule      = false;
	Required->bFinalUpdateModule = false;

	Required->EmitterOrigin     = FVector::ZeroVector;
	Required->EmitterRotation   = FRotator::ZeroRotator;
	Required->bUseLocalSpace    = false;
	Required->bKillOnCompleted  = false;
	Required->bKillOnDeactivate = false;

	Required->EmitterDuration    = 1.0f;
	Required->EmitterDurationLow = 1.0f;
	Required->EmitterDelay       = 0.0f;
	Required->EmitterLoops       = 0;

	Required->ScreenAlignment      = PSA_FacingCameraPosition;
	Required->SortMode             = PSORTMODE_None;
	Required->SubImages_Horizontal = 1;
	Required->SubImages_Vertical   = 1;
	Required->SpawnRate            = 10.0f;

	LOD->RequiredModule = Required;

	UParticleModuleSpawn* Spawn = UObjectManager::Get().CreateObject<UParticleModuleSpawn>(LOD);

	Spawn->bEnabled           = true;
	Spawn->bSpawnModule       = true;
	Spawn->bUpdateModule      = false;
	Spawn->bFinalUpdateModule = false;

	Spawn->SpawnRate      = 20.0f;
	Spawn->SpawnRateScale = 1.0f;
	Spawn->BurstScale     = 1.0f;

	LOD->SpawnModule = Spawn;

	UParticleModuleLifetime* Lifetime = UObjectManager::Get().CreateObject<UParticleModuleLifetime>(LOD);

	Lifetime->bEnabled           = true;
	Lifetime->bSpawnModule       = true;
	Lifetime->bUpdateModule      = false;
	Lifetime->bFinalUpdateModule = false;

	Lifetime->LifetimeMin = 1.0f;
	Lifetime->LifetimeMax = 1.0f;

	LOD->Modules.push_back(Lifetime);

	UParticleModuleLocation* Location = UObjectManager::Get().CreateObject<UParticleModuleLocation>(LOD);

	Location->bEnabled           = true;
	Location->bSpawnModule       = true;
	Location->bUpdateModule      = true;
	Location->bFinalUpdateModule = false;
	Location->StartLocation      = FVector::ZeroVector;

	LOD->Modules.push_back(Location);

	UParticleModuleVelocity* Velocity = UObjectManager::Get().CreateObject<UParticleModuleVelocity>(LOD);

	Velocity->bEnabled           = true;
	Velocity->bSpawnModule       = true;
	Velocity->bUpdateModule      = true;
	Velocity->bFinalUpdateModule = false;
	Velocity->MinVelocity        = FVector(0.0f, 0.f, 20.0f);
	Velocity->MaxVelocity        = FVector(0.0f, 0.f, 80.0f);

	LOD->Modules.push_back(Velocity);

	UParticleModuleSize* Size = UObjectManager::Get().CreateObject<UParticleModuleSize>(LOD);

	Size->bEnabled           = true;
	Size->bSpawnModule       = true;
	Size->bUpdateModule      = true;
	Size->bFinalUpdateModule = false;
	Size->StartSize          = FVector(1.0f, 1.0f, 1.0f);

	LOD->Modules.push_back(Size);

	UParticleModuleColor* Color = UObjectManager::Get().CreateObject<UParticleModuleColor>(LOD);

	Color->bEnabled           = true;
	Color->bSpawnModule       = true;
	Color->bUpdateModule      = true;
	Color->bFinalUpdateModule = false;
	Color->StartColor         = FColor::White();
	Color->StartAlpha         = 1.0f;
	Color->bClampAlpha        = true;

	LOD->Modules.push_back(Color);

	LOD->UpdateModuleLists();

	LODLevels.push_back(LOD);

	CacheEmitterModuleInfo();
}

bool UParticleEmitter::HasValidLOD0() const
{
	UParticleLODLevel* LOD0 = GetLODLevel(0);
	return LOD0 && LOD0->RequiredModule && LOD0->SpawnModule;
}
