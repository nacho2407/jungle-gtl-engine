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
#include "Color/ParticleModuleColorOverLife.h"
#include "Particles/Rotation/ParticleModuleMeshRotation.h"
#include "Particles/RotationRate/ParticleModuleMeshRotationRate.h"

namespace
{
    void ApplySharedModule(UParticleModule*& Target, UParticleModule* Shared)
    {
        if (!Shared || Target == Shared)
        {
            return;
        }
        if (Target)
        {
            UObjectManager::Get().DestroyObject(Target);
        }
        Target = Shared;
    }

    void SerializeLODShareFlags(FArchive& Ar, UParticleLODLevel* LOD, UParticleLODLevel* HigherLOD)
    {
        bool bShareRequired = Ar.IsSaving() && LOD && HigherLOD && LOD->RequiredModule == HigherLOD->RequiredModule;
        Ar << bShareRequired;
        if (Ar.IsLoading() && bShareRequired && LOD && HigherLOD)
        {
            UParticleModule* Target = LOD->RequiredModule;
            ApplySharedModule(Target, HigherLOD->RequiredModule);
            LOD->RequiredModule = Cast<UParticleModuleRequired>(Target);
        }

        bool bShareSpawn = Ar.IsSaving() && LOD && HigherLOD && LOD->SpawnModule == HigherLOD->SpawnModule;
        Ar << bShareSpawn;
        if (Ar.IsLoading() && bShareSpawn && LOD && HigherLOD)
        {
            UParticleModule* Target = LOD->SpawnModule;
            ApplySharedModule(Target, HigherLOD->SpawnModule);
            LOD->SpawnModule = Cast<UParticleModuleSpawn>(Target);
        }

        bool bShareTypeData = Ar.IsSaving() && LOD && HigherLOD && LOD->TypeDataModule == HigherLOD->TypeDataModule;
        Ar << bShareTypeData;
        if (Ar.IsLoading() && bShareTypeData && LOD && HigherLOD)
        {
            UParticleModule* Target = static_cast<UParticleModule*>(LOD->TypeDataModule);
            ApplySharedModule(Target, static_cast<UParticleModule*>(HigherLOD->TypeDataModule));
            LOD->TypeDataModule = Cast<UParticleModuleTypeDataBase>(Target);
        }

        uint32 ModuleCount = Ar.IsSaving() && LOD ? static_cast<uint32>(LOD->Modules.size()) : 0;
        Ar << ModuleCount;
        for (uint32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
        {
            bool bShareModule = Ar.IsSaving() && LOD && HigherLOD && ModuleIndex < HigherLOD->Modules.size() && LOD->
            Modules[ModuleIndex] == HigherLOD->Modules[ModuleIndex];
            Ar << bShareModule;
            if (Ar.IsLoading() && bShareModule && LOD && HigherLOD && ModuleIndex < LOD->Modules.size() && ModuleIndex <
                HigherLOD->Modules.size())
            {
                ApplySharedModule(LOD->Modules[ModuleIndex], HigherLOD->Modules[ModuleIndex]);
            }
        }

        if (Ar.IsLoading() && LOD)
        {
            LOD->UpdateModuleLists();
        }
    }
}

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
	bMeshRotationActive = false;

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
		HighTypeData->CacheModuleInfo(this);

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

		if (Cast<UParticleModuleMeshRotation>(ParticleModule) || Cast<UParticleModuleMeshRotationRate>(ParticleModule))
		{
			bMeshRotationActive = true;
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
	// 버전 1: bEnabled/bUseMeshInstance만 저장하고 로드 시 DefaultSpriteEmitter로 리셋되던
	//          이전 포맷. 모듈 편집이 디스크에 들어가지 않아 사실상 사용 불가.
    // 버전 2: 이름/피벗/초기 할당량 + LODLevels 전부를 직렬화.
    // 버전 3: sub-LOD 모듈 공유 포인터를 저장/복원.
    int32 Version = 3;
	Ar << Version;

	bool bSavedEnabled         = bEnabled;
	bool bSavedUseMeshInstance = bUseMeshInstance;
	Ar << bSavedEnabled;
	Ar << bSavedUseMeshInstance;

	if (Ar.IsLoading())
	{
		bEnabled         = bSavedEnabled;
		bUseMeshInstance = bSavedUseMeshInstance;
	}

	if (Version < 2)
	{
		// 옛 포맷: 모듈 데이터가 없으므로 기본 이미터 구축으로 폴백.
		if (Ar.IsLoading() && !bUseMeshInstance)
		{
			InitializeDefaultSpriteEmitter();
		}
		return;
	}

	Ar << EmitterName;
	Ar << InitialAllocationCount;
	Ar << QualityLevelSpawnRateScale;
	Ar << PivotOffset;

	uint32 LODCount = Ar.IsSaving() ? static_cast<uint32>(LODLevels.size()) : 0;
	Ar << LODCount;

	if (Ar.IsLoading())
	{
		LODLevels.clear();
		LODLevels.resize(LODCount, nullptr);
	}

	for (uint32 i = 0; i < LODCount; ++i)
	{
		bool bValid = Ar.IsSaving() ? (LODLevels[i] != nullptr) : false;
		Ar << bValid;
		if (!bValid)
		{
			continue;
		}

		if (Ar.IsLoading())
		{
			LODLevels[i] = UObjectManager::Get().CreateObject<UParticleLODLevel>(this);
		}
		LODLevels[i]->Serialize(Ar);
        if (Version >= 3)
        {
            UParticleLODLevel* HigherLOD = i > 0 ? LODLevels[i - 1] : nullptr;
            SerializeLODShareFlags(Ar, LODLevels[i], HigherLOD);
        }
	}

	if (Ar.IsLoading())
	{
		if (LODLevels.empty() && !bUseMeshInstance)
		{
			// 파일에 LOD가 없으면 기본 스프라이트 이미터로 채워서 항상 유효한 상태 유지.
			InitializeDefaultSpriteEmitter();
		}
		else
		{
			CacheEmitterModuleInfo();
		}
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
	Required->bUseMaxDrawCount     = false;
	Required->MaxDrawCount         = 0;

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
	Location->bUpdateModule      = false;
	Location->bFinalUpdateModule = false;

	Location->StartLocation.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(Location);
	if (UDistributionVectorUniform* Uniform = Cast<UDistributionVectorUniform>(Location->StartLocation.Distribution))
	{
		Uniform->Min = FVector(-1.0f, -1.f, -1.0f);
		Uniform->Max = FVector(1.0f, 1.f, 1.0f);
	}

	LOD->Modules.push_back(Location);

	UParticleModuleVelocity* Velocity = UObjectManager::Get().CreateObject<UParticleModuleVelocity>(LOD);

	Velocity->bEnabled           = true;
	Velocity->bSpawnModule       = true;
	Velocity->bUpdateModule      = false;
	Velocity->bFinalUpdateModule = false;

	Velocity->StartVelocity.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(Velocity);
	if (UDistributionVectorUniform* Uniform = Cast<UDistributionVectorUniform>(Velocity->StartVelocity.Distribution))
	{
		Uniform->Min = FVector(-1.0f, -1.f, -1.0f);
		Uniform->Max = FVector(1.0f, 1.f, 1.0f);
	}

	Velocity->StartVelocityRadial.Distribution = UObjectManager::Get().CreateObject<UDistributionFloatUniform>(Velocity);
	if (UDistributionFloatUniform* Uniform = Cast<UDistributionFloatUniform>(Velocity->StartVelocityRadial.Distribution))
	{
		Uniform->Min = 0.0f;
		Uniform->Max = 0.0f;
	}

	LOD->Modules.push_back(Velocity);

	UParticleModuleSize* Size = UObjectManager::Get().CreateObject<UParticleModuleSize>(LOD);

	Size->bEnabled           = true;
	Size->bSpawnModule       = true;
	Size->bUpdateModule      = false;
	Size->bFinalUpdateModule = false;
	Size->StartSize.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(Size);
	if (UDistributionVectorUniform* Uniform = Cast<UDistributionVectorUniform>(Size->StartSize.Distribution))
	{
		Uniform->Min = FVector(0.0f, 0.f, 0.0f);
		Uniform->Max = FVector(50.0f, 50.f, 50.0f);
	}

	LOD->Modules.push_back(Size);

	UParticleModuleColor* Color = UObjectManager::Get().CreateObject<UParticleModuleColor>(LOD);

	Color->bEnabled           = true;
	Color->bSpawnModule       = true;
	Color->bUpdateModule      = false;
	Color->bFinalUpdateModule = false;
	Color->bClampAlpha        = true;

	Color->StartColor.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(Color);
	if (UDistributionVectorConstant* Constant = Cast<UDistributionVectorConstant>(Color->StartColor.Distribution))
	{
		Constant->Constant = FVector(1, 1, 1);
	}

	Color->StartAlpha.Distribution = UObjectManager::Get().CreateObject<UDistributionFloatConstant>();
	if (UDistributionFloatConstant* Constant = Cast<UDistributionFloatConstant>(Color->StartAlpha.Distribution))
	{
		Constant->Constant = 1.0f;
	}

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

