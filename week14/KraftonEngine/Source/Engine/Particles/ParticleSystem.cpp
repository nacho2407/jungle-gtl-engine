#include "ParticleSystem.h"

#include "Particles/Module/ParticleModule.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Serialization/Archive.h"

UParticleLODLevel::~UParticleLODLevel()
{
	for (UParticleModule* Module : Modules)
	{
		delete Module;
	}
	Modules.clear();
}

void UParticleLODLevel::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << Level;
	Ar << bEnabled;

	uint32 ModuleCount = 0;
	Ar << ModuleCount;

	if (Ar.IsLoading())
	{
		for (UParticleModule* Module : Modules)
		{
			delete Module;
		}
		Modules.clear();

		// Module subclasses are not serialized yet. Consume the count now so the
		// package format can grow without changing the LOD header layout.
		Modules.reserve(ModuleCount);
	}
}

UParticleEmitter::~UParticleEmitter()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		delete LODLevel;
	}
	LODLevels.clear();
}

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* Component)
{
	FParticleEmitterInstance* Instance = new FParticleEmitterInstance();
	Instance->Init(this, Component);
	return Instance;
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(LODLevels.size()))
	{
		return nullptr;
	}

	return LODLevels[Index];
}

void UParticleEmitter::AddLODLevel(UParticleLODLevel* LODLevel)
{
	if (!LODLevel)
	{
		return;
	}

	LODLevels.push_back(LODLevel);
}

void UParticleEmitter::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << MaxActiveParticles;
	Ar << EmitterDuration;
	Ar << bLooping;

	uint32 LODLevelCount = Ar.IsSaving() ? static_cast<uint32>(LODLevels.size()) : 0;
	Ar << LODLevelCount;

	if (Ar.IsLoading())
	{
		for (UParticleLODLevel* LODLevel : LODLevels)
		{
			delete LODLevel;
		}
		LODLevels.clear();
		LODLevels.reserve(LODLevelCount);
	}

	for (uint32 Index = 0; Index < LODLevelCount; ++Index)
	{
		UParticleLODLevel* LODLevel = Ar.IsSaving() ? LODLevels[Index] : new UParticleLODLevel();
		LODLevel->Serialize(Ar);

		if (Ar.IsLoading())
		{
			LODLevels.push_back(LODLevel);
		}
	}
}

UParticleSystem::~UParticleSystem()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		delete Emitter;
	}
	Emitters.clear();
}

void UParticleSystem::InitializeDefaultEmitters()
{
	UParticleEmitter* DefaultEmitter = UObjectManager::Get().CreateObject<UParticleEmitter>();
	DefaultEmitter->SetMaxActiveParticles(100);
	DefaultEmitter->SetEmitterDuration(1.0f);
	DefaultEmitter->SetLooping(true);

	UParticleLODLevel* DefaultLOD = UObjectManager::Get().CreateObject<UParticleLODLevel>();
	DefaultLOD->SetLevel(0);
	DefaultLOD->SetEnabled(true);
	DefaultLOD->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleRequired>());
	DefaultLOD->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleSpawn>());
	DefaultLOD->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleLifetime>());
	DefaultLOD->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleLocation>());
	DefaultLOD->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleVelocity>());
	DefaultLOD->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleColor>());
	DefaultLOD->GetMutableModules().push_back(UObjectManager::Get().CreateObject<UParticleModuleSize>());

	DefaultEmitter->AddLODLevel(DefaultLOD);

	AddEmitter(DefaultEmitter);
}

void UParticleSystem::AddEmitter(UParticleEmitter* Emitter)
{
	if (!Emitter)
	{
		return;
	}

	Emitters.push_back(Emitter);
}

void UParticleSystem::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << AssetPathFileName;

	uint32 EmitterCount = Ar.IsSaving() ? static_cast<uint32>(Emitters.size()) : 0;
	Ar << EmitterCount;

	if (Ar.IsLoading())
	{
		for (UParticleEmitter* Emitter : Emitters)
		{
			delete Emitter;
		}
		Emitters.clear();
		Emitters.reserve(EmitterCount);
	}

	for (uint32 Index = 0; Index < EmitterCount; ++Index)
	{
		UParticleEmitter* Emitter = Ar.IsSaving() ? Emitters[Index] : new UParticleEmitter();
		Emitter->Serialize(Ar);

		if (Ar.IsLoading())
		{
			Emitters.push_back(Emitter);
		}
	}
}
