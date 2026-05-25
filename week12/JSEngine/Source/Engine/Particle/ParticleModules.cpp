#include "Particle/ParticleModules.h"

#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleHelper.h"

namespace
{
	bool IsLiveObject(const UObject* Object)
	{
		return Object != nullptr && UObjectManager::Get().ContainsObject(Object);
	}

	int32 AlignParticleBytes(int32 Value)
	{
		return ParticleHelper::AlignParticleSize(Value);
	}

	void AddParticlePayloadOffset(
		FParticleLODLevelRuntimeCache& Cache,
		UParticleModule* Module,
		UParticleModuleTypeDataBase* TypeData,
		int32& InOutParticleBytes)
	{
		if (Module == nullptr)
		{
			return;
		}

		const int32 RequiredBytes = Module->RequiredBytes(TypeData);
		if (RequiredBytes <= 0)
		{
			return;
		}

		InOutParticleBytes = AlignParticleBytes(InOutParticleBytes);
		Cache.ModulePayloadOffsets[Module] = InOutParticleBytes;
		InOutParticleBytes += RequiredBytes;
	}

	void AddInstancePayloadOffset(
		FParticleLODLevelRuntimeCache& Cache,
		UParticleModule* Module,
		UParticleModuleTypeDataBase* TypeData,
		int32& InOutInstancePayloadSize)
	{
		if (Module == nullptr)
		{
			return;
		}

		const int32 RequiredBytes = Module->RequiredBytesPerInstance(TypeData);
		if (RequiredBytes <= 0)
		{
			return;
		}

		InOutInstancePayloadSize = AlignParticleBytes(InOutInstancePayloadSize);
		Cache.ModuleInstanceOffsets[Module] = InOutInstancePayloadSize;
		InOutInstancePayloadSize += RequiredBytes;
	}

	FParticleLODLevelRuntimeCache BuildLODLevelRuntimeCache(const UParticleLODLevel* LODLevel)
	{
		FParticleLODLevelRuntimeCache Cache;
		Cache.PayloadOffset = AlignParticleBytes(static_cast<int32>(sizeof(FBaseParticle)));

		int32 ParticleBytes = Cache.PayloadOffset;
		int32 InstancePayloadSize = 0;

		if (LODLevel == nullptr)
		{
			Cache.ParticleStride = AlignParticleBytes(ParticleBytes);
			Cache.InstancePayloadSize = AlignParticleBytes(InstancePayloadSize);
			return Cache;
		}

		Cache.RequiredModule = LODLevel->RequiredModule;
		Cache.SpawnModule = LODLevel->SpawnModule;
		Cache.TypeDataModule = LODLevel->TypeDataModule;

		if (!LODLevel->bEnabled)
		{
			Cache.ParticleStride = AlignParticleBytes(ParticleBytes);
			Cache.InstancePayloadSize = AlignParticleBytes(InstancePayloadSize);
			return Cache;
		}

		UParticleModuleTypeDataBase* TypeData = Cache.TypeDataModule;
		if (TypeData != nullptr && TypeData->bEnabled)
		{
			const int32 TypeDataPayloadSize = TypeData->GetRequiredPayloadSize();
			if (TypeDataPayloadSize > 0)
			{
				ParticleBytes = AlignParticleBytes(ParticleBytes);
				ParticleBytes += TypeDataPayloadSize;
			}

			AddInstancePayloadOffset(Cache, TypeData, TypeData, InstancePayloadSize);
		}

		if (Cache.SpawnModule == nullptr)
		{
			for (UParticleModule* Module : LODLevel->Modules)
			{
				if (Module != nullptr && Module->bEnabled && Module->IsSpawnRateModule())
				{
					Cache.SpawnModule = Cast<UParticleModuleSpawn>(Module);
					break;
				}
			}
		}

		// Required / SpawnModule은 Modules 배열과 별개의 특수 모듈이므로 먼저 offset만 계산
		if (Cache.RequiredModule != nullptr && Cache.RequiredModule->bEnabled)
		{
			AddParticlePayloadOffset(Cache, Cache.RequiredModule, TypeData, ParticleBytes);
			AddInstancePayloadOffset(Cache, Cache.RequiredModule, TypeData, InstancePayloadSize);
		}
		if (Cache.SpawnModule != nullptr && Cache.SpawnModule->bEnabled)
		{
			AddParticlePayloadOffset(Cache, Cache.SpawnModule, TypeData, ParticleBytes);
			AddInstancePayloadOffset(Cache, Cache.SpawnModule, TypeData, InstancePayloadSize);
		}

		for (UParticleModule* Module : LODLevel->Modules)
		{
			if (Module == nullptr || !Module->bEnabled || Module == Cache.RequiredModule || Module == Cache.SpawnModule || Module == Cache.TypeDataModule)
			{
				continue;
			}

			if (Module->IsSpawnModule())
			{
				Cache.SpawnModules.push_back(Module);
			}

			if (Module->IsUpdateModule())
			{
				Cache.UpdateModules.push_back(Module);
			}

			AddParticlePayloadOffset(Cache, Module, TypeData, ParticleBytes);
			AddInstancePayloadOffset(Cache, Module, TypeData, InstancePayloadSize);
		}

		Cache.ParticleStride = AlignParticleBytes(ParticleBytes);
		Cache.InstancePayloadSize = AlignParticleBytes(InstancePayloadSize);
		return Cache;
	}
}

int32 UParticleModule::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return 0;
}

int32 UParticleModule::RequiredBytesPerInstance(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return 0;
}

bool UParticleModule::IsSpawnRateModule() const
{
	return false;
}

bool UParticleModule::IsSpawnModule() const
{
	return false;
}

bool UParticleModule::IsUpdateModule() const
{
	return false;
}

void UParticleModule::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)Owner;
	(void)Offset;
	(void)DeltaTime;
}

void UParticleModule::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)Owner;
	(void)Offset;
	(void)DeltaTime;
}

bool UParticleModuleSpawn::IsSpawnRateModule() const
{
	return true;
}

UParticleModuleLifetime::UParticleModuleLifetime()
{
	Lifetime.Constant = 1.0f;
	Lifetime.Min = 1.0f;
	Lifetime.Max = 1.0f;
}

bool UParticleModuleLifetime::IsSpawnModule() const
{
	return true;
}

bool UParticleModuleLocation::IsSpawnModule() const
{
	return true;
}

bool UParticleModuleVelocity::IsSpawnModule() const
{
	return true;
}

UParticleModuleColor::UParticleModuleColor()
{
	StartColor.Constant = FColor::White();
	StartColor.Min = FColor::White();
	StartColor.Max = FColor::White();
}

bool UParticleModuleColor::IsSpawnModule() const
{
	return true;
}

UParticleModuleSize::UParticleModuleSize()
{
	StartSize.Constant = FVector::OneVector;
	StartSize.Min = FVector::OneVector;
	StartSize.Max = FVector::OneVector;
}

bool UParticleModuleSize::IsSpawnModule() const
{
	return true;
}

bool UParticleModuleCollision::IsUpdateModule() const
{
	return true;
}

UParticleLODLevel::~UParticleLODLevel()
{
	if (IsLiveObject(RequiredModule))
	{
		UObjectManager::Get().DestroyObject(RequiredModule);
	}
	RequiredModule = nullptr;

	if (IsLiveObject(SpawnModule))
	{
		UObjectManager::Get().DestroyObject(SpawnModule);
	}
	SpawnModule = nullptr;

	for (UParticleModule* Module : Modules)
	{
		if (Module != RequiredModule && Module != SpawnModule && Module != TypeDataModule && IsLiveObject(Module))
		{
			UObjectManager::Get().DestroyObject(Module);
		}
	}
	Modules.clear();

	if (IsLiveObject(TypeDataModule))
	{
		UObjectManager::Get().DestroyObject(TypeDataModule);
	}
	TypeDataModule = nullptr;
}

UParticleEmitter::~UParticleEmitter()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (IsLiveObject(LODLevel))
		{
			UObjectManager::Get().DestroyObject(LODLevel);
		}
	}
	LODLevels.clear();
}

UParticleSystem::~UParticleSystem()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (IsLiveObject(Emitter))
		{
			UObjectManager::Get().DestroyObject(Emitter);
		}
	}
	Emitters.clear();
}

void UParticleModuleTypeDataBase::Build()
{
}

FParticleEmitterInstance* UParticleModuleTypeDataBase::CreateInstance(
	UParticleEmitter* InEmitterTemplate,
	IParticleEmitterInstanceOwner& InOwner)
{
	FParticleEmitterInstance* Instance = new FParticleEmitterInstance(InOwner);
	Instance->SpriteTemplate = InEmitterTemplate;
	return Instance;
}

FDynamicEmitterDataBase* UParticleModuleTypeDataBase::GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance)
{
	(void)InEmitterInstance;
	return nullptr;
}

int32 UParticleModuleTypeDataBase::GetRequiredPayloadSize() const
{
	return 0;
}

int32 FParticleLODLevelRuntimeCache::GetParticlePayloadOffset(UParticleModule* Module) const
{
	const auto It = ModulePayloadOffsets.find(Module);
	return It != ModulePayloadOffsets.end() ? It->second : -1;
}

int32 FParticleLODLevelRuntimeCache::GetInstancePayloadOffset(UParticleModule* Module) const
{
	const auto It = ModuleInstanceOffsets.find(Module);
	return It != ModuleInstanceOffsets.end() ? It->second : -1;
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	LODLevelRuntimeCaches.clear();
	LODLevelRuntimeCaches.reserve(LODLevels.size());

	for (const UParticleLODLevel* LODLevel : LODLevels)
	{
		FParticleLODLevelRuntimeCache Cache = BuildLODLevelRuntimeCache(LODLevel);
		LODLevelRuntimeCaches.push_back(Cache);
	}
}

TArray<int32> UParticleEmitter::CalculateTotalPayloadSize() const
{
	TArray<int32> Result;
	Result.reserve(LODLevels.size());

	for (const UParticleLODLevel* LODLevel : LODLevels)
	{
		Result.push_back(BuildLODLevelRuntimeCache(LODLevel).ParticleStride);
	}

	return Result;
}

FParticleLODLevelRuntimeCache* UParticleEmitter::GetLODLevelRuntimeCache(int32 LODIndex)
{
	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevelRuntimeCaches.size()))
	{
		return nullptr;
	}

	return &LODLevelRuntimeCaches[LODIndex];
}

const FParticleLODLevelRuntimeCache* UParticleEmitter::GetLODLevelRuntimeCache(int32 LODIndex) const
{
	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevelRuntimeCaches.size()))
	{
		return nullptr;
	}

	return &LODLevelRuntimeCaches[LODIndex];
}

FParticleLODLevelRuntimeCache* UParticleEmitter::GetLOD0RuntimeCache()
{
	return GetLODLevelRuntimeCache(0);
}

const FParticleLODLevelRuntimeCache* UParticleEmitter::GetLOD0RuntimeCache() const
{
	return GetLODLevelRuntimeCache(0);
}
